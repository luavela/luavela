/*
 * Machine code management.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "lj_obj.h"
#if LJ_HASJIT
#include "lj_vm.h"
#include "uj_errmsg.h"
#include "jit/lj_jit.h"
#include "jit/lj_trace.h"
#endif

/* -- OS-specific functions ----------------------------------------------- */

#if LJ_HASJIT || LJ_HASFFI

/* Define this if you want to run LuaJIT with Valgrind. */
#ifdef UJIT_USE_VALGRIND
#include <valgrind/valgrind.h>
#endif

/* Synchronize data/instruction cache. */
void lj_mcode_sync(void *start, void *end) {
#ifdef UJIT_USE_VALGRIND
  /* When encountering new piece of target code, Valgrind core translates
  ** it into an internal structure and stores it in a cache. When re-executing
  ** already cached piece of code, the saved structure is used. Such structure
  ** is not directly linked to the target code, so, if target code changes,
  ** Valgrind translation remains the same.
  ** This macro flushes translation for the trace (either just created, or patched).
  ** If the trace is just created, this has no effect on Valgrind core, since the
  ** translation was not yet cached. In case of patching, this macro causes Valgrind
  ** to reload and re-translate the trace on next execution.
  ** Effectively, if this line is missing, Valgrind will always execute initial states
  ** of root traces, never taking any compiled side exits.
  */
  VALGRIND_DISCARD_TRANSLATIONS(start, (char *)end-(char *)start);
#endif
  UNUSED(start); UNUSED(end);
}

#endif

#if LJ_HASJIT

#include <sys/mman.h>

/* Define this ONLY if the page protection twiddling becomes a bottleneck. */
#ifndef LUAJIT_UNPROTECT_MCODE
#include "uj_dispatch.h"
#endif /* !LUAJIT_UNPROTECT_MCODE */

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

#define MCPROT_RW   (PROT_READ|PROT_WRITE)
#define MCPROT_RX   (PROT_READ|PROT_EXEC)
#define MCPROT_RWX  (PROT_READ|PROT_WRITE|PROT_EXEC)

static void *mcode_alloc_at(jit_State *J, uintptr_t hint, size_t sz, int prot) {
  void *p = mmap((void *)hint, sz, prot, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  if (p == MAP_FAILED) {
    if (!hint) { lj_trace_err(J, LJ_TRERR_MCODEAL); }
    p = NULL;
  }
  return p;
}

static void mcode_free(jit_State *J, void *p, size_t sz) {
  UNUSED(J);
  munmap(p, sz);
}

/* -- MCode area protection ----------------------------------------------- */

#ifdef LUAJIT_UNPROTECT_MCODE

/* It's generally considered to be a potential security risk to have
** pages with simultaneous write *and* execute access in a process.
**
** Do not even think about using this mode for server processes or
** apps handling untrusted external data (such as a browser).
**
** The security risk is not in LuaJIT itself -- but if an adversary finds
** any *other* flaw in your C application logic, then any RWX memory page
** simplifies writing an exploit considerably.
*/
#define MCPROT_GEN  MCPROT_RWX
#define MCPROT_RUN  MCPROT_RWX

static void mcode_protect(jit_State *J, int prot) {
  UNUSED(J); UNUSED(prot);
}

#else

/* This is the default behaviour and much safer:
**
** Most of the time the memory pages holding machine code are executable,
** but NONE of them is writable.
**
** The current memory area is marked read-write (but NOT executable) only
** during the short time window while the assembler generates machine code.
*/
#define MCPROT_GEN  MCPROT_RW
#define MCPROT_RUN  MCPROT_RX

/* Protection twiddling failed. Probably due to kernel security. */
static LJ_NOINLINE void mcode_protfail(jit_State *J) {
  lua_CFunction panic = J2G(J)->panic;
  if (panic) {
    lua_State *L = J->L;
    setstrV(L, L->top++, uj_errmsg_str(L, UJ_ERR_JITPROT));
    panic(L);
  }
}

static void mcode_setprot(jit_State *J, void *p, size_t sz, int prot) {
  if (LJ_UNLIKELY(mprotect(p, sz, prot))) {
    mcode_protfail(J);
  }
}

/* Change protection of MCode area. */
static void mcode_protect(jit_State *J, int prot) {
  if (J->mcprot == prot) { return; }

  mcode_setprot(J, J->mcarea, J->szmcarea, prot);
  J->mcprot = prot;
}

#endif

/* -- MCode area allocation ----------------------------------------------- */

#define mcode_validptr(p)  (p)

#ifdef UJ_TARGET_JUMPRANGE

/* Get memory within relative jump distance of our code in 64 bit mode. */
static void *mcode_alloc(jit_State *J, size_t sz) {
  /* Target an address in the static assembler code (64K aligned).
  ** Try addresses within a distance of target-range/2+1MB..target+range/2-1MB.
  ** Use half the jump range so every address in the range can reach any other.
  */
UJ_PEDANTIC_OFF /* casting a function ptr to void* */
  uintptr_t target = (uintptr_t)(void *)lj_vm_exit_handler & ~(uintptr_t)0xffff;
UJ_PEDANTIC_ON
  const uintptr_t range = (1u << (UJ_TARGET_JUMPRANGE-1)) - (1u << 21);
  /* First try a contiguous area below the last one. */
  uintptr_t hint = J->mcarea ? (uintptr_t)J->mcarea - sz : 0;
  int i;
  /* Limit probing iterations, depending on the available pool size. */
  for (i = 0; i < UJ_TARGET_JUMPRANGE; i++) {
    if (mcode_validptr(hint)) {
      void *p = mcode_alloc_at(J, hint, sz, MCPROT_GEN);

      if (mcode_validptr(p) &&
          ((uintptr_t)p + sz - target < range || target - (uintptr_t)p < range)) {
        return p;
      }
      if (p) { mcode_free(J, p, sz); } /* Free badly placed area. */
    }
    /* Next try probing 64K-aligned pseudo-random addresses. */
    do {
      hint = LJ_PRNG_BITS(J, UJ_TARGET_JUMPRANGE-16) << 16;
    } while (!(hint + sz < range+range));
    hint = target + hint - range;
  }
  lj_trace_err(J, LJ_TRERR_MCODEAL);  /* Give up. OS probably ignores hints? */
  return NULL;
}

#else

/* All memory addresses are reachable by relative jumps. */
#define mcode_alloc(J, sz)  mcode_alloc_at((J), 0, (sz), MCPROT_GEN)

#endif

/* -- MCode area management ----------------------------------------------- */

/* Linked list of MCode areas. */
typedef struct MCLink {
  MCode *next;  /* Next area. */
  size_t size;  /* Size of current area. */
} MCLink;

/* Allocate a new MCode area. */
static void mcode_allocarea(jit_State *J) {
  MCode *oldarea = J->mcarea;
  size_t sz = (size_t)J->param[JIT_P_sizemcode] << 10;
  sz = (sz + UJ_PAGESIZE-1) & ~(size_t)(UJ_PAGESIZE - 1);
  J->mcarea = (MCode *)mcode_alloc(J, sz);
  J->szmcarea = sz;
  J->mcprot = MCPROT_GEN;
  J->mctop = (MCode *)((char *)J->mcarea + J->szmcarea);
  J->mcbot = (MCode *)((char *)J->mcarea + sizeof(MCLink));
  ((MCLink *)J->mcarea)->next = oldarea;
  ((MCLink *)J->mcarea)->size = sz;
  J->szallmcarea += sz;
}

/* Free all MCode areas. */
void lj_mcode_free(jit_State *J) {
  MCode *mc = J->mcarea;
  J->mcarea = NULL;
  J->szallmcarea = 0;
  while (mc) {
    MCode *next = ((MCLink *)mc)->next;
    mcode_free(J, mc, ((MCLink *)mc)->size);
    mc = next;
  }
}

/* -- MCode transactions -------------------------------------------------- */

/* Reserve the remainder of the current MCode area. */
MCode *lj_mcode_reserve(jit_State *J, MCode **lim) {
  if (!J->mcarea) {
    mcode_allocarea(J);
  } else {
    mcode_protect(J, MCPROT_GEN);
  }
  *lim = J->mcbot;
  return J->mctop;
}

/* Commit the top part of the current MCode area. */
void lj_mcode_commit(jit_State *J, MCode *top) {
  J->mctop = top;
  mcode_protect(J, MCPROT_RUN);
}

/* Abort the reservation. */
void lj_mcode_abort(jit_State *J) {
  if (J->mcarea) {
    mcode_protect(J, MCPROT_RUN);
  }
}

void lj_mcode_patch_finish(jit_State *J, MCode *ptr) {
#ifdef LUAJIT_UNPROTECT_MCODE
  UNUSED(J); UNUSED(ptr);
#else
  if (J->mcarea == ptr) {
    mcode_protect(J, MCPROT_RUN);
  } else {
    mcode_setprot(J, ptr, ((MCLink *)ptr)->size, MCPROT_RUN);
  }
#endif
}

MCode* lj_mcode_patch_start(jit_State *J, MCode *ptr) {
#ifdef LUAJIT_UNPROTECT_MCODE
  UNUSED(J); UNUSED(ptr);
  return NULL;
#else
  MCode *mc = J->mcarea;
  /* Try current area first to use the protection cache. */
  if (ptr >= mc && ptr < (MCode *)((char *)mc + J->szmcarea)) {
    mcode_protect(J, MCPROT_GEN);
    return mc;
  }
  /* Otherwise search through the list of MCode areas. */
  for (;;) {
    mc = ((MCLink *)mc)->next;
    lua_assert(mc != NULL);
    if (ptr >= mc && ptr < (MCode *)((char *)mc + ((MCLink *)mc)->size)) {
      mcode_setprot(J, mc, ((MCLink *)mc)->size, MCPROT_GEN);
      return mc;
    }
  }
#endif
}

/* Limit of MCode reservation reached. */
void lj_mcode_limiterr(jit_State *J, size_t need) {
  size_t sizemcode, maxmcode;
  lj_mcode_abort(J);
  sizemcode = (size_t)J->param[JIT_P_sizemcode] << 10;
  sizemcode = (sizemcode + UJ_PAGESIZE-1) & ~(size_t)(UJ_PAGESIZE - 1);
  maxmcode = (size_t)J->param[JIT_P_maxmcode] << 10;
  if ((size_t)need > sizemcode) {
    lj_trace_err(J, LJ_TRERR_MCODEOV);  /* Too long for any area. */
  }
  if (J->szallmcarea + sizemcode > maxmcode) {
    lj_trace_err(J, LJ_TRERR_MCODEAL);
  }
  mcode_allocarea(J);
  lj_trace_err(J, LJ_TRERR_MCODELM);  /* Retry with new area. */
}

#endif
