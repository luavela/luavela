/*
 * Bytecode reader.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "lj_obj.h"
#include "uj_obj_immutable.h"
#include "uj_mem.h"
#include "uj_errmsg.h"
#include "uj_throw.h"
#include "uj_str.h"
#include "uj_sbuf.h"
#include "lj_tab.h"
#include "lj_bc.h"
#if LJ_HASFFI
#include "ffi/lj_ctype.h"
#include "ffi/lj_cdata.h"
#include "lextlib.h"
#endif
#include "frontend/lj_lex.h"
#include "lj_bcdump.h"
#include "uj_state.h"

#include "utils/leb128.h"

/* Reuse some lexer fields for our own purposes. */
#define bcread_flags(ls)        (ls)->level
#define bcread_oldtop(L, ls)    uj_state_stack_restore((L), (ls)->lastline)
#define bcread_savetop(L, ls, top) \
  (ls)->lastline = (BCLine)uj_state_stack_save((L), (top))

/* -- Input buffer handling ----------------------------------------------- */

/* Throw reader error. */
static LJ_NOINLINE void bcread_error(LexState *ls, enum err_msg em)
{
  lua_State *L = ls->L;
  const char *name = ls->chunkarg;
  if (*name == BCDUMP_HEAD1) name = "(binary)";
  else if (*name == '@' || *name == '=') name++;
  uj_str_pushf(L, "%s: %s", name, uj_errmsg(em));
  uj_throw(L, LUA_ERRSYNTAX);
}

/* Refill buffer if needed. */
static LJ_NOINLINE void bcread_fill(LexState *ls, size_t len, int need)
{
  struct sbuf *sb = &ls->sb;
  lua_assert(len != 0);

  if (len > LJ_MAX_MEM || ls->current < 0) {
    bcread_error(ls, UJ_ERR_BCBAD);
  }

  do {
    const char *buf;
    size_t size;
    if (ls->n) {  /* Copy remainder to buffer. */
      if (uj_sbuf_size(sb) != 0) {  /* Move down in buffer. */
        lua_assert(ls->p + ls->n == uj_sbuf_back(sb));
        if (ls->n != uj_sbuf_size(sb)) {
	  /* NB!: Cannot use reset + push (memcpy) because of overlapping memory */
          memmove(sb->buf, ls->p, ls->n);
          sb->sz = ls->n;
        }
      } else {  /* Copy from buffer provided by reader. */
        uj_sbuf_push_block(sb, ls->p, ls->n);
      }
      ls->p = uj_sbuf_front(sb);
    } else {
      uj_sbuf_reset(sb);
    }
    buf = ls->rfunc(ls->L, ls->rdata, &size);  /* Get more data from reader. */
    if (buf == NULL || size == 0) {  /* EOF? */
      if (need) bcread_error(ls, UJ_ERR_BCBAD);
      ls->current = -1;  /* Only bad if we get called again. */
      break;
    }
    if (uj_sbuf_size(sb) != 0) {  /* Append to buffer. */
      uj_sbuf_push_block(sb, buf, size);
      ls->n = uj_sbuf_size(sb);
      ls->p = uj_sbuf_front(sb);
    } else {  /* Return buffer provided by reader. */
      ls->n = size;
      ls->p = buf;
    }
  } while (ls->n < len);
}

/* Need a certain number of bytes. */
static LJ_AINLINE void bcread_need(LexState *ls, size_t len)
{
  if (LJ_UNLIKELY(ls->n < len))
    bcread_fill(ls, len, 1);
}

/* Want to read up to a certain number of bytes, but may need less. */
static LJ_AINLINE void bcread_want(LexState *ls, size_t len)
{
  if (LJ_UNLIKELY(ls->n < len))
    bcread_fill(ls, len, 0);
}

#define bcread_dec(ls)          lua_check((ls)->n > 0, (ls)->n--)
#define bcread_consume(ls, len) lua_check((ls)->n >= (len), (ls)->n -= (len))

/* Return memory block from buffer. */
static uint8_t *bcread_mem(LexState *ls, size_t len)
{
  uint8_t *p = (uint8_t *)ls->p;
  bcread_consume(ls, len);
  ls->p = (char *)p + len;
  return p;
}

/* Copy memory block from buffer. */
static void bcread_block(LexState *ls, void *q, size_t len)
{
  memcpy(q, bcread_mem(ls, len), len);
}

/* Read byte from buffer. */
static LJ_AINLINE uint32_t bcread_byte(LexState *ls)
{
  bcread_dec(ls);
  return (uint32_t)(uint8_t)*ls->p++;
}

/* Read ULEB128 value from buffer. */

static uint64_t bcread_qword(LexState *ls)
{
  uint64_t value;
  size_t n = read_uleb128_n(&value, (const uint8_t *)ls->p, ls->n);
  lua_assert(n > 0);
  ls->p += n;
  ls->n -= n;
  return value;
}

static uint32_t bcread_dword(LexState *ls)
{
  return (uint32_t)bcread_qword(ls);
}

/* -- Bytecode reader ----------------------------------------------------- */

/* Read debug info of a prototype. */
static void bcread_dbg(LexState *ls, GCproto *pt, size_t sizedbg)
{
  void *lineinfo = (void *)proto_lineinfo(pt);
  bcread_block(ls, lineinfo, sizedbg);
}

/* Find pointer to varinfo. */
static uint8_t* bcread_varinfo(GCproto *pt)
{
  uint8_t *p = proto_uvinfo(pt);
  size_t n = pt->sizeuv;
  if (n) while (*p++ || --n) ;
  return p;
}

/* Read a single constant key/value of a template table. */
static void bcread_ktabk(LexState *ls, TValue *o)
{
  uint32_t tp = bcread_dword(ls);
  if (tp >= BCDUMP_KTAB_STR) {
    size_t len = tp - BCDUMP_KTAB_STR;
    const char *p = (const char *)bcread_mem(ls, len);
    setstrV(ls->L, o, uj_str_new(ls->L, p, len));
  } else if (tp == BCDUMP_KTAB_INT) {
    setintV(o, (int32_t)bcread_dword(ls));
  } else if (tp == BCDUMP_KTAB_NUM) {
    setrawV(o, bcread_qword(ls));
  } else {
    lua_assert(tp <= BCDUMP_KTAB_TRUE);
    settag(o, ~tp);
  }
}

/* Read a template table. */
static GCtab *bcread_ktab(LexState *ls)
{
  uint32_t narray = bcread_dword(ls);
  uint32_t nhash = bcread_dword(ls);
  GCtab *t = lj_tab_new(ls->L, narray, hsize2hbits(nhash));
  if (narray) {  /* Read array entries. */
    uint32_t i;
    TValue *o = t->array;
    for (i = 0; i < narray; i++, o++)
      bcread_ktabk(ls, o);
  }
  if (nhash) {  /* Read hash entries. */
    uint32_t i;
    for (i = 0; i < nhash; i++) {
      TValue key;
      bcread_ktabk(ls, &key);
      lua_assert(!tvisnil(&key));
      bcread_ktabk(ls, lj_tab_set(ls->L, t, &key));
    }
  }
  return t;
}

/* Read GC constants of a prototype. */
static void bcread_kgc(LexState *ls, GCproto *pt, size_t sizekgc)
{
  size_t i;
  GCobj **kr = (GCobj**)(pt->k) - (ptrdiff_t)sizekgc;
  for (i = 0; i < sizekgc; i++, kr++) {
    size_t tp = bcread_dword(ls);
    if (tp >= BCDUMP_KGC_STR) {
      size_t len = tp - BCDUMP_KGC_STR;
      const char *p = (const char *)bcread_mem(ls, len);
      *kr = obj2gco(uj_str_new(ls->L, p, len));
    } else if (tp == BCDUMP_KGC_TAB) {
      *kr = obj2gco(bcread_ktab(ls));
#if LJ_HASFFI
    } else if (tp != BCDUMP_KGC_CHILD) {
      CTypeID id = tp == BCDUMP_KGC_COMPLEX ? CTID_COMPLEX_DOUBLE :
                   tp == BCDUMP_KGC_I64 ? CTID_INT64 : CTID_UINT64;
      CTSize sz = tp == BCDUMP_KGC_COMPLEX ? 16 : 8;
      GCcdata *cd = lj_cdata_new_(ls->L, id, sz);
      TValue *p = (TValue *)cdataptr(cd);
      *kr = obj2gco(cd);
      setrawV(&p[0], bcread_qword(ls));
      if (tp == BCDUMP_KGC_COMPLEX) {
        setrawV(&p[1], bcread_qword(ls));
      }
#endif
    } else {
      lua_State *L = ls->L;
      lua_assert(tp == BCDUMP_KGC_CHILD);
      if (L->top <= bcread_oldtop(L, ls))  /* Stack underflow? */
        bcread_error(ls, UJ_ERR_BCBAD);
      L->top--;
      *kr = obj2gco(protoV(L->top));
    }
  }
}

/* Read number constants of a prototype. */
static void bcread_knum(LexState *ls, GCproto *pt, size_t sizekn)
{
  size_t i;
  TValue *o = (TValue*)pt->k;
  for (i = 0; i < sizekn; i++, o++) {
    setrawV(o, bcread_qword(ls));
  }
}

/* Read bytecode instructions. */
static void bcread_bytecode(LexState *ls, GCproto *pt, size_t sizebc)
{
  BCIns *bc = proto_bc(pt);
  bc[0] = BCINS_AD((pt->flags & PROTO_VARARG) ? BC_FUNCV : BC_FUNCF,
                   pt->framesize, 0);
  bcread_block(ls, bc+1, (sizebc-1)*sizeof(BCIns));
}

/* Read upvalue refs. */
static void bcread_uv(LexState *ls, GCproto *pt, size_t sizeuv)
{
  if (sizeuv) {
    uint16_t *uv = proto_uv(pt);
    bcread_block(ls, uv, sizeuv*2);
  }
}

/* Read a prototype. */
static GCproto *bcread_proto(LexState *ls)
{
  GCproto *pt;
  size_t framesize, numparams, flags, sizeuv, sizekgc, sizekn, sizebc, sizept;
  size_t ofsk, ofsuv, ofsdbg;
  size_t sizedbg = 0;
  BCLine firstline = 0, numline = 0;
  size_t len, startn;

  /* Read length. */
  if (ls->n > 0 && ls->p[0] == 0) {  /* Shortcut EOF. */
    ls->n--; ls->p++;
    return NULL;
  }
  bcread_want(ls, 5);
  len = bcread_dword(ls);
  if (!len) return NULL;  /* EOF */
  bcread_need(ls, len);
  startn = ls->n;

  /* Read prototype header. */
  flags = bcread_byte(ls);
  numparams = bcread_byte(ls);
  framesize = bcread_byte(ls);
  sizeuv = bcread_byte(ls);
  sizekgc = bcread_dword(ls);
  sizekn = bcread_dword(ls);
  sizebc = bcread_dword(ls) + 1;
  if (!(bcread_flags(ls) & BCDUMP_F_STRIP)) {
    sizedbg = bcread_dword(ls);
    if (sizedbg) {
      firstline = bcread_dword(ls);
      numline = bcread_dword(ls);
    }
  }

  /* Calculate total size of prototype including all colocated arrays. */
  sizept = sizeof(GCproto) +
           sizebc*sizeof(BCIns) +
           sizekgc*sizeof(GCobj*);
  sizept = (sizept + sizeof(TValue)-1) & ~(sizeof(TValue)-1);
  ofsk = sizept; sizept += sizekn*sizeof(TValue);
  ofsuv = sizept; sizept += ((sizeuv+1)&~1)*2;
  ofsdbg = sizept; sizept += sizedbg;

  /* Allocate prototype object and initialize its fields. */
  pt = (GCproto *)uj_obj_new(ls->L, sizept);
  pt->gct = ~LJ_TPROTO;
  pt->numparams = (uint8_t)numparams;
  pt->framesize = (uint8_t)framesize;
  pt->sizebc = sizebc;
  pt->k = (void *)((char *)pt + ofsk);
  pt->uv = (uint16_t *)((char *)pt + ofsuv);
  pt->sizekgc = 0;  /* Set to zero until fully initialized. */
  pt->sizekn = sizekn;
  pt->sizept = sizept;
  pt->sizeuv = (uint8_t)sizeuv;
  pt->flags = (uint8_t)flags;
  pt->trace = 0;
  pt->chunkname = ls->chunkname;
#ifdef UJIT_PROFILER
  pt->profcount = 0;
#endif /* UJIT_PROFILER */
  uj_obj_immutable_set_mark(obj2gco(pt));

  /* Close potentially uninitialized gap between bc and kgc. */
  *(uint32_t *)((char *)pt + ofsk - sizeof(GCobj*)*(sizekgc+1)) = 0;

  /* Read bytecode instructions and upvalue refs. */
  bcread_bytecode(ls, pt, sizebc);
  bcread_uv(ls, pt, sizeuv);

  /* Read constants. */
  bcread_kgc(ls, pt, sizekgc);
  pt->sizekgc = sizekgc;
  bcread_knum(ls, pt, sizekn);

  /* Read and initialize debug info. */
  pt->firstline = firstline;
  pt->numline = numline;
  if (sizedbg) {
    size_t sizeli = (sizebc-1) << (numline < 256 ? 0 : numline < 65536 ? 1 : 2);
    pt->lineinfo = (void*)((char *)pt + ofsdbg);
    pt->uvinfo = (uint8_t *)((char *)pt + ofsdbg + sizeli);
    bcread_dbg(ls, pt, sizedbg);
    pt->varinfo = bcread_varinfo(pt);
  } else {
    pt->lineinfo = NULL;
    pt->uvinfo = NULL;
    pt->varinfo = NULL;
  }

  if (len != startn - ls->n)
    bcread_error(ls, UJ_ERR_BCBAD);
  return pt;
}

/* Read and check header of bytecode dump. */
static int bcread_header(LexState *ls)
{
  uint32_t flags;
  bcread_want(ls, 3+5+5);
  if (bcread_byte(ls) != BCDUMP_HEAD2 ||
      bcread_byte(ls) != BCDUMP_HEAD3 ||
      bcread_byte(ls) != BCDUMP_VERSION) return 0;
  bcread_flags(ls) = flags = bcread_dword(ls);
  if ((flags & ~(BCDUMP_F_KNOWN)) != 0) return 0;
  if ((flags & BCDUMP_F_FFI)) {
#if LJ_HASFFI
    lua_State *L = ls->L;
    if (!ctype_ctsG(G(L))) {
      ptrdiff_t oldtop = uj_state_stack_save(L, L->top);
      luaopen_ffi(L);  /* Load FFI library on-demand. */
      L->top = uj_state_stack_restore(L, oldtop);
    }
#else
    return 0;
#endif
  }
  if ((flags & BCDUMP_F_STRIP)) {
    ls->chunkname = uj_str_newz(ls->L, ls->chunkarg);
  } else {
    size_t len = bcread_dword(ls);
    bcread_need(ls, len);
    ls->chunkname = uj_str_new(ls->L, (const char *)bcread_mem(ls, len), len);
  }
  return 1;  /* Ok. */
}

/* Read a bytecode dump. */
GCproto *lj_bcread(LexState *ls)
{
  lua_State *L = ls->L;
  lua_assert(ls->current == BCDUMP_HEAD1);
  bcread_savetop(L, ls, L->top);
  uj_sbuf_reset(&ls->sb);
  /* Check for a valid bytecode dump header. */
  if (!bcread_header(ls))
    bcread_error(ls, UJ_ERR_BCFMT);
  for (;;) {  /* Process all prototypes in the bytecode dump. */
    GCproto *pt = bcread_proto(ls);
    if (!pt) break;
    setprotoV(L, L->top, pt);
    uj_state_stack_incr_top(L);
  }
  if ((int32_t)ls->n > 0 || L->top-1 != bcread_oldtop(L, ls))
    bcread_error(ls, UJ_ERR_BCBAD);
  /* Pop off last prototype. */
  L->top--;
  return protoV(L->top);
}

