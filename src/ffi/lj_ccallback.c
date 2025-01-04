/*
 * FFI C callback handling.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "lj_obj.h"

#if LJ_HASFFI

#include "lj_gc.h"
#include "uj_mem.h"
#include "uj_err.h"
#include "uj_errmsg.h"
#include "lj_tab.h"
#include "uj_state.h"
#include "lj_frame.h"
#include "ffi/lj_ctype.h"
#include "ffi/lj_cconv.h"
#include "ffi/lj_ccall.h"
#include "ffi/lj_ccallback.h"
#include "jit/lj_target.h"
#include "jit/lj_mcode.h"
#include "jit/lj_trace.h"
#include "lj_vm.h"
#include "uj_cframe.h"

/* -- Target-specific handling of callback slots -------------------------- */

#define CALLBACK_MCODE_SIZE     (UJ_PAGESIZE * LJ_NUM_CBPAGE)

#define CALLBACK_MCODE_HEAD     8
#define CALLBACK_MCODE_GROUP    (-2+1+2+5+6)

#define CALLBACK_SLOT2OFS(slot) \
  (CALLBACK_MCODE_HEAD + CALLBACK_MCODE_GROUP*((slot)/32) + 4*(slot))

static size_t CALLBACK_OFS2SLOT(size_t ofs)
{
  size_t group;
  ofs -= CALLBACK_MCODE_HEAD;
  group = ofs / (32*4 + CALLBACK_MCODE_GROUP);
  return (ofs % (32*4 + CALLBACK_MCODE_GROUP))/4 + group*32;
}

#define CALLBACK_MAX_SLOT \
  (((CALLBACK_MCODE_SIZE-CALLBACK_MCODE_HEAD)/(CALLBACK_MCODE_GROUP+4*32))*32)

/* Convert callback slot number to callback function pointer. */
static void *callback_slot2ptr(CTState *cts, size_t slot)
{
  return (uint8_t *)cts->cb.mcode + CALLBACK_SLOT2OFS(slot);
}

/* Convert callback function pointer to slot number. */
size_t lj_ccallback_ptr2slot(CTState *cts, void *p)
{
  uintptr_t ofs = (uintptr_t)((uint8_t *)p -(uint8_t *)cts->cb.mcode);
  if (ofs < CALLBACK_MCODE_SIZE) {
    size_t slot = CALLBACK_OFS2SLOT((size_t)ofs);
    if (CALLBACK_SLOT2OFS(slot) == (size_t)ofs)
      return slot;
  }
  return ~0u;  /* Not a known callback function pointer. */
}

/* Initialize machine code for callback function pointers. */
static void callback_mcode_init(global_State *g, uint8_t *page)
{
  uint8_t *p = page;
UJ_PEDANTIC_OFF /* casting a function ptr to void* */
  uint8_t *target = (uint8_t *)(void *)lj_vm_ffi_callback;
UJ_PEDANTIC_ON
  size_t slot;
  *(void **)p = target; p += 8;
  for (slot = 0; slot < CALLBACK_MAX_SLOT; slot++) {
    /* mov al, slot; jmp group */
    *p++ = XI_MOVrib | RID_EAX; *p++ = (uint8_t)slot;
    if ((slot & 31) == 31 || slot == CALLBACK_MAX_SLOT-1) {
      /* push ebp/rbp; mov ah, slot>>8; mov ebp, &g. */
      *p++ = XI_PUSH + RID_EBP;
      *p++ = XI_MOVrib | (RID_EAX+4); *p++ = (uint8_t)(slot >> 8);
      *p++ = XI_MOVri | RID_EBP;
      *(int32_t *)p = i32ptr(g); p += 4;
      /* jmp [rip-pageofs] where lj_vm_ffi_callback is stored. */
      *p++ = XI_GROUP5; *p++ = XM_OFS0 + (XOg_JMP<<3) + RID_EBP;
      *(int32_t *)p = (int32_t)(page-(p+4)); p += 4;
    } else {
      *p++ = XI_JMPs; *p++ = (uint8_t)((2+2)*(31-(slot&31)) - 2);
    }
  }
  lua_assert(p - page <= CALLBACK_MCODE_SIZE);
}

/* -- Machine code management --------------------------------------------- */
#include <sys/mman.h>
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS   MAP_ANON
#endif

/* Allocate and initialize area for callback function pointers. */
static void callback_mcode_new(CTState *cts)
{
  size_t sz = (size_t)CALLBACK_MCODE_SIZE;
  void *p;
  if (CALLBACK_MAX_SLOT == 0)
    uj_err_caller(cts->L, UJ_ERR_FFI_CBACKOV);
  p = mmap(NULL, sz, (PROT_READ|PROT_WRITE), MAP_PRIVATE|MAP_ANONYMOUS,
           -1, 0);
  if (p == MAP_FAILED)
    uj_err_caller(cts->L, UJ_ERR_FFI_CBACKOV);
  cts->cb.mcode = p;
  callback_mcode_init(cts->g, p);
  lj_mcode_sync(p, (char *)p + sz);
  mprotect(p, sz, (PROT_READ|PROT_EXEC));
}

/* Free area for callback function pointers. */
void lj_ccallback_mcode_free(CTState *cts)
{
  size_t sz = (size_t)CALLBACK_MCODE_SIZE;
  void *p = cts->cb.mcode;
  if (p == NULL) return;
  munmap(p, sz);
}

/* -- C callback entry ---------------------------------------------------- */

/* Target-specific handling of register arguments. Similar to lj_ccall.c. */
#define CALLBACK_HANDLE_REGARG \
  if (isfp) { \
    if (nfpr + n <= CCALL_NARG_FPR) { \
      sp = &cts->cb.fpr[nfpr]; \
      nfpr += n; \
      goto done; \
    } \
  } else { \
    if (ngpr + n <= maxgpr) { \
      sp = &cts->cb.gpr[ngpr]; \
      ngpr += n; \
      goto done; \
    } \
  }

/* Convert and push callback arguments to Lua stack. */
static void callback_conv_args(CTState *cts, lua_State *L)
{
  TValue *o = L->top;
  intptr_t *stack = cts->cb.stack;
  size_t slot = cts->cb.slot;
  CTypeID id = 0, rid, fid;
  int gcsteps = 0;
  CType *ct;
  GCfunc *fn;
  size_t ngpr = 0, nsp = 0, maxgpr = CCALL_NARG_GPR;
#if CCALL_NARG_FPR
  size_t nfpr = 0;
#endif

  if (slot < cts->cb.sizeid && (id = cts->cb.cbid[slot]) != 0) {
    ct = ctype_get(cts, id);
    rid = ctype_cid(ct->info);
    fn = funcV(lj_tab_getint(cts->miscmap, (int32_t)slot));
  } else {  /* Must set up frame first, before throwing the error. */
    ct = NULL;
    rid = 0;
    fn = (GCfunc *)L;
  }
  o->u32.lo = LJ_CONT_FFI_CALLBACK;  /* Continuation returns from callback. */
  o->u32.hi = rid;  /* Return type. x86: +(spadj<<16). */
  o++;
  setframe_gc(o, obj2gco(fn));
  setframe_ftsz(o, (int)((char *)(o+1) - (char *)L->base) + FRAME_CONT);
  L->top = L->base = ++o;
  if (!ct)
    uj_err_caller(cts->L, UJ_ERR_FFI_BADCBACK);
  if (isluafunc(fn))
    uj_cframe_pc_set(L->cframe, proto_bc(funcproto(fn))+1);
  uj_state_stack_check(L, LUA_MINSTACK);  /* May throw. */
  o = L->base;  /* Might have been reallocated. */

  fid = ct->sib;
  while (fid) {
    CType *ctf = ctype_get(cts, fid);
    if (!ctype_isattrib(ctf->info)) {
      CType *cta;
      void *sp;
      CTSize sz;
      int isfp;
      size_t n;
      lua_assert(ctype_isfield(ctf->info));
      cta = ctype_rawchild(cts, ctf);
      isfp = ctype_isfp(cta->info);
      sz = (cta->size + CTSIZE_PTR-1) & ~(CTSIZE_PTR-1);
      n = sz / CTSIZE_PTR;  /* Number of GPRs or stack slots needed. */

      CALLBACK_HANDLE_REGARG  /* Handle register arguments. */

      /* Otherwise pass argument on stack. */
      sp = &stack[nsp];
      nsp += n;

    done:
      gcsteps += lj_cconv_tv_ct(cts, cta, 0, o++, sp);
    }
    fid = ctf->sib;
  }
  L->top = o;
  while (gcsteps-- > 0)
    lj_gc_check(L);
}

/* Convert Lua object to callback result. */
static void callback_conv_result(CTState *cts, lua_State *L, TValue *o)
{
  CType *ctr = ctype_raw(cts, (uint16_t)(L->base-2)->u32.hi);
  if (!ctype_isvoid(ctr->info)) {
    uint8_t *dp = (uint8_t *)&cts->cb.gpr[0];
#if CCALL_NUM_FPR
    if (ctype_isfp(ctr->info))
      dp = (uint8_t *)&cts->cb.fpr[0];
#endif
    lj_cconv_ct_tv(cts, ctr, dp, o, 0);
#ifdef CALLBACK_HANDLE_RET
    CALLBACK_HANDLE_RET
#endif
    /* Extend returned integers to (at least) 32 bits. */
    if (ctype_isinteger_or_bool(ctr->info) && ctr->size < 4) {
      if (ctr->info & CTF_UNSIGNED)
        *(uint32_t *)dp = ctr->size == 1 ? (uint32_t)*(uint8_t *)dp :
                                           (uint32_t)*(uint16_t *)dp;
      else
        *(int32_t *)dp = ctr->size == 1 ? (int32_t)*(int8_t *)dp :
                                          (int32_t)*(int16_t *)dp;
    }
  }
}

/* Enter callback. */
lua_State * lj_ccallback_enter(CTState *cts, void *cf)
{
  lua_State *L = cts->L;
  global_State *g = cts->g;
  lua_assert(L != NULL);
  if (g->jit_L != NULL) {
    setstrV(L, L->top++, uj_errmsg_str(L, UJ_ERR_FFI_BADCBACK));
    if (g->panic) g->panic(L);
    exit(EXIT_FAILURE);
  }
  lj_trace_abort(g);  /* Never record across callback. */
  /* Setup C frame. */
  uj_cframe_prev_set(cf, L->cframe);
  uj_cframe_L_set(cf, L);
  uj_cframe_errfunc_inherit(cf);
  uj_cframe_nres_set(cf, 0);
  L->cframe = cf;
  callback_conv_args(cts, L);
  return L;  /* Now call the function on this stack. */
}

/* Leave callback. */
void lj_ccallback_leave(CTState *cts, TValue *o)
{
  lua_State *L = cts->L;
  GCfunc *fn;
  TValue *obase = L->base;
  L->base = L->top;  /* Keep continuation frame for throwing errors. */
  if (o >= L->base) {
    /* PC of RET* is lost. Point to last line for result conv. errors. */
    fn = curr_func(L);
    if (isluafunc(fn)) {
      GCproto *pt = funcproto(fn);
      uj_cframe_pc_set(L->cframe, proto_bc(pt)+pt->sizebc+1);
    }
  }
  callback_conv_result(cts, L, o);
  /* Finally drop C frame and continuation frame. */
  L->cframe = uj_cframe_prev(L->cframe);
  L->top -= 2;
  L->base = obase;
  cts->cb.slot = 0;  /* Blacklist C function that called the callback. */
}

/* -- C callback management ----------------------------------------------- */

/* Get an unused slot in the callback slot table. */
static size_t callback_slot_new(CTState *cts, CType *ct)
{
  CTypeID id = ctype_typeid(cts, ct);
  CTypeID1 *cbid = cts->cb.cbid;
  size_t top;
  for (top = cts->cb.topid; top < cts->cb.sizeid; top++)
    if (LJ_LIKELY(cbid[top] == 0))
      goto found;
#if CALLBACK_MAX_SLOT
  if (top >= CALLBACK_MAX_SLOT)
#endif
    uj_err_caller(cts->L, UJ_ERR_FFI_CBACKOV);
  if (!cts->cb.mcode)
    callback_mcode_new(cts);
  cbid = (CTypeID1 *)uj_mem_grow(cts->L, cbid,
                        &(cts->cb.sizeid), CALLBACK_MAX_SLOT, sizeof(CTypeID1));
  cts->cb.cbid = cbid;
  memset(cbid+top, 0, (cts->cb.sizeid-top)*sizeof(CTypeID1));
found:
  cbid[top] = id;
  cts->cb.topid = top+1;
  return top;
}

/* Check for function pointer and supported argument/result types. */
static CType *callback_checkfunc(CTState *cts, CType *ct)
{
  int narg = 0;
  if (!ctype_isptr(ct->info) || (ct->size != CTSIZE_PTR))
    return NULL;
  ct = ctype_rawchild(cts, ct);
  if (ctype_isfunc(ct->info)) {
    CType *ctr = ctype_rawchild(cts, ct);
    CTypeID fid = ct->sib;
    if (!(ctype_isvoid(ctr->info) || ctype_isenum(ctr->info) ||
          ctype_isptr(ctr->info) || (ctype_isnum(ctr->info) && ctr->size <= 8)))
      return NULL;
    if ((ct->info & CTF_VARARG))
      return NULL;
    while (fid) {
      CType *ctf = ctype_get(cts, fid);
      if (!ctype_isattrib(ctf->info)) {
        CType *cta;
        lua_assert(ctype_isfield(ctf->info));
        cta = ctype_rawchild(cts, ctf);
        if (!(ctype_isenum(cta->info) || ctype_isptr(cta->info) ||
              (ctype_isnum(cta->info) && cta->size <= 8)) ||
            ++narg >= LUA_MINSTACK-3)
          return NULL;
      }
      fid = ctf->sib;
    }
    return ct;
  }
  return NULL;
}

/* Create a new callback and return the callback function pointer. */
void *lj_ccallback_new(CTState *cts, CType *ct, GCfunc *fn)
{
  ct = callback_checkfunc(cts, ct);
  if (ct) {
    size_t slot = callback_slot_new(cts, ct);
    GCtab *t = cts->miscmap;
    setfuncV(cts->L, lj_tab_setint(cts->L, t, (int32_t)slot), fn);
    lj_gc_anybarriert(cts->L, t);
    return callback_slot2ptr(cts, slot);
  }
  return NULL;  /* Bad conversion. */
}

#endif
