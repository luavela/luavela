/*
 * SSA IR (Intermediate Representation) emitter.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

/* For pointers to libc/libm functions. */
#include <stdio.h>
#include <math.h>

#include "lj_obj.h"

#if LJ_HASJIT

#include "uj_dispatch.h"
#include "uj_mem.h"
#include "uj_str.h"
#include "lj_tab.h"
#include "jit/lj_ir.h"
#include "jit/lj_jit.h"
#include "jit/lj_ircall.h"
#include "jit/lj_iropt.h"
#include "jit/lj_trace.h"
#if LJ_HASFFI
#include "ffi/lj_ctype.h"
#include "ffi/lj_cdata.h"
#include "ffi/lj_carith.h"
#endif
#include "uj_lib.h"

/* Some local macros to save typing. Undef'd at the end. */
#define IR(ref)                 (&J->cur.ir[(ref)])
#define fins                    (&J->fold.ins)

/* Pass IR on to next optimization in chain (FOLD). */
#define emitir(ot, a, b)        (lj_ir_set(J, (ot), (a), (b)), lj_opt_fold(J))

/* -- IR tables ----------------------------------------------------------- */

/* IR instruction modes. */
LJ_DATADEF const uint8_t lj_ir_mode[IR__MAX+1] = {
IRDEF(IRMODE)
  0
};

/* IR type sizes. */
LJ_DATADEF const uint8_t lj_ir_type_size[IRT__MAX+1] = {
#define IRTSIZE(name, size)     size,
IRTDEF(IRTSIZE)
#undef IRTSIZE
  0
};

/* C call info for CALL* instructions. */
LJ_DATADEF const CCallInfo lj_ir_callinfo[] = {
#define IRCALLCI(cond, name, nargs, kind, type, flags) \
  { (ASMFunction)IRCALLCOND_##cond(name), \
    (nargs)|(CCI_CALL_##kind)|(IRT_##type<<CCI_OTSHIFT)|(flags) },
IRCALLDEF(IRCALLCI)
#undef IRCALLCI
  { NULL, 0 }
};

/* -- IR emitter ---------------------------------------------------------- */

/* Grow IR buffer at the top. */
void lj_ir_growtop(jit_State *J)
{
  IRIns *baseir = J->irbuf + J->irbotlim;
  size_t szins = J->irtoplim - J->irbotlim;
  if (szins) {
    size_t osize = szins * sizeof(IRIns);
    baseir = (IRIns *)uj_mem_realloc(J->L, baseir, osize, osize << 1);
    J->irtoplim = J->irbotlim + 2*szins;
  } else {
    baseir = (IRIns *)uj_mem_alloc(J->L, LJ_MIN_IRSZ * sizeof(IRIns));
    J->irbotlim = REF_BASE - LJ_MIN_IRSZ/4;
    J->irtoplim = J->irbotlim + LJ_MIN_IRSZ;
  }
  J->cur.ir = J->irbuf = baseir - J->irbotlim;
}

/* Grow IR buffer at the bottom or shift it up. */
static void lj_ir_growbot(jit_State *J)
{
  IRIns *baseir = J->irbuf + J->irbotlim;
  size_t szins = J->irtoplim - J->irbotlim;
  lua_assert(szins != 0);
  lua_assert(J->cur.nk == J->irbotlim);
  if (J->cur.nins + (szins >> 1) < J->irtoplim) {
    /* More than half of the buffer is free on top: shift up by a quarter. */
    size_t ofs = szins >> 2;
    memmove(baseir + ofs, baseir, (J->cur.nins - J->irbotlim)*sizeof(IRIns));
    J->irbotlim -= ofs;
    J->irtoplim -= ofs;
    J->cur.ir = J->irbuf = baseir - J->irbotlim;
  } else {
    /* Double the buffer size, but split the growth amongst top/bottom. */
    IRIns *newbase = (IRIns *)uj_mem_alloc(J->L, 2 * szins * sizeof(IRIns));
    size_t ofs = szins >= 256 ? 128 : (szins >> 1);  /* Limit bottom growth. */
    memcpy(newbase + ofs, baseir, (J->cur.nins - J->irbotlim)*sizeof(IRIns));
    uj_mem_free(MEM(J->L), baseir, szins * sizeof(*baseir));
    J->irbotlim -= ofs;
    J->irtoplim = J->irbotlim + 2*szins;
    J->cur.ir = J->irbuf = newbase - J->irbotlim;
  }
}

/* Emit IR without any optimizations. */
TRef lj_ir_emit(jit_State *J)
{
  IRRef ref = lj_ir_nextins(J);
  IRIns *ir = IR(ref);
  IROp op = fins->o;
  ir->prev = J->chain[op];
  J->chain[op] = (IRRef1)ref;
  ir->o = op;
  ir->op1 = fins->op1;
  ir->op2 = fins->op2;
  ir->hints = fins->hints;
  J->guardemit.irt |= fins->t.irt;
  return TREF(ref, irt_t((ir->t = fins->t)));
}

/* Emit call to a C function. */
TRef lj_ir_call_(jit_State *J, int id, ...)
{
  const CCallInfo *ci = &lj_ir_callinfo[id];
  uint32_t n = CCI_NARGS(ci);
  TRef tr = TREF_NIL;
  va_list argp;
  va_start(argp, id);
  if ((ci->flags & CCI_L)) n--;
  if (n > 0)
    tr = va_arg(argp, IRRef);
  while (n-- > 1)
    tr = emitir(IRT(IR_CARG, IRT_NIL), tr, va_arg(argp, IRRef));
  va_end(argp);
  if (CCI_OP(ci) == IR_CALLS)
    J->needsnap = 1;  /* Need snapshot after call with side effect. */
  return emitir(CCI_OPTYPE(ci), tr, id);
}

/*
 * Emits a guard to ensure that a given tab is mutable.
 * If this is not true, side exit to the interpreter is taken.
 * Implementation note. Emitted BAND + EQ are effectively fused into
 * test + jnz, at least under x86_64.
 * No-op if we can prove that tab is mutable and abort trace in case
 * it is surely immutable.
 */
void lj_ir_emit_immutable_guard(jit_State *J, const TRef tab) {
  TRef value;
  const TRef field = emitir(IRT(IR_FLOAD, IRT_U8), tab, IRFL_GCO_MARKED);
  IRIns *ir = IR(tref_ref(tab));

  /* TNEW is obviously mutable, TDUP doesn't copy immutability */
  if (ir->o == IR_TNEW || ir->o == IR_TDUP) {
    if (!lj_opt_fwd_immutability_guard(J, tab, field))
      lj_trace_err(J, LJ_TRERR_GFAIL);
    else
      return; /* field ref will be removed by DCE */
  }

  value = emitir(IRT(IR_BAND, IRT_U8), field, lj_ir_kint(J, UJ_GCO_IMMUTABLE));
  emitir(IRTGI(IR_EQ), value, lj_ir_kint(J, 0));
}

/* -- Interning of constants ---------------------------------------------- */

/*
** IR instructions for constants are kept between J->cur.nk >= ref < REF_BIAS.
** They are chained like all other instructions, but grow downwards.
** The are interned (like strings in the VM) to facilitate reference
** comparisons. The same constant must get the same reference.
*/

/* Get ref of next IR constant and optionally grow IR.
** Note: this may invalidate all IRIns *!
*/
static LJ_AINLINE IRRef ir_nextk(jit_State *J)
{
  IRRef ref = J->cur.nk;
  if (LJ_UNLIKELY(ref <= J->irbotlim)) lj_ir_growbot(J);
  J->cur.nk = --ref;
  return ref;
}

/* Intern int32_t constant. */
TRef lj_ir_kint(jit_State *J, int32_t k)
{
  IRIns *ir, *cir = J->cur.ir;
  IRRef ref;
  for (ref = J->chain[IR_KINT]; ref; ref = cir[ref].prev)
    if (cir[ref].i == k)
      goto found;
  ref = ir_nextk(J);
  ir = IR(ref);
  ir->i = k;
  ir->t.irt = IRT_INT;
  ir->o = IR_KINT;
  ir->prev = J->chain[IR_KINT];
  J->chain[IR_KINT] = (IRRef1)ref;
found:
  return TREF(ref, IRT_INT);
}

/* The MRef inside the KNUM/KINT64 IR instructions holds the address of the
** 64 bit constant. The constants themselves are stored in a chained array
** and shared across traces.
**
** Rationale for choosing this data structure:
** - The address of the constants is embedded in the generated machine code
**   and must never move. A resizable array or hash table wouldn't work.
** - Most apps need very few non-32 bit integer constants (less than a dozen).
** - Linear search is hard to beat in terms of speed and low complexity.
*/
struct K64Array {
  K64Array* next;       /* Pointer to next list. */
  size_t numk;                  /* Number of used elements in this array. */
  TValue k[LJ_MIN_K64SZ];       /* Array of constants. */
};

/* Free all chained arrays. */
void lj_ir_k64_freeall(jit_State *J)
{
  K64Array *k;
  for (k = J->k64; k; ) {
    K64Array *next = k->next;
    uj_mem_free(MEM_G(J2G(J)), k, sizeof(*k));
    k = next;
  }
}

/* Find 64 bit constant in chained array or add it. */
const TValue *lj_ir_k64_find(jit_State *J, uint64_t u64)
{
  K64Array *k, *kp = NULL;
  TValue *ntv;
  size_t idx;
  /* Search for the constant in the whole chain of arrays. */
  for (k = J->k64; k; k = k->next) {
    kp = k;  /* Remember previous element in list. */
    for (idx = 0; idx < k->numk; idx++) {  /* Search one array. */
      TValue *tv = &k->k[idx];
      if (rawV(tv) == u64)  /* Needed for +-0/NaN/absmask. */
        return tv;
    }
  }
  /* Constant was not found, need to add it. */
  if (!(kp && kp->numk < LJ_MIN_K64SZ)) {  /* Allocate a new array. */
    K64Array *kn = (K64Array *)uj_mem_alloc(J->L, sizeof(K64Array));
    kn->next = NULL;
    kn->numk = 0;
    if (kp)
      kp->next = kn;  /* Chain to the end of the list. */
    else
      J->k64 = kn;  /* Link first array. */
    kp = kn;
  }
  ntv = &kp->k[kp->numk++];  /* Add to current array. */
  setrawV(ntv, u64);
  return ntv;
}

/* Intern 64 bit constant, given by its address. */
TRef lj_ir_k64(jit_State *J, IROp op, const TValue *tv)
{
  IRIns *ir, *cir = J->cur.ir;
  IRRef ref;
  IRType t = op == IR_KNUM ? IRT_NUM : IRT_I64;
  for (ref = J->chain[op]; ref; ref = cir[ref].prev)
    if (ir_k64(&cir[ref]) == tv)
      goto found;
  ref = ir_nextk(J);
  ir = IR(ref);
  ir->ptr = (void*)tv;
  ir->t.irt = t;
  ir->o = op;
  ir->prev = J->chain[op];
  J->chain[op] = (IRRef1)ref;
found:
  return TREF(ref, t);
}

/* Intern FP constant, given by its 64 bit pattern. */
TRef lj_ir_knum_u64(jit_State *J, uint64_t u64)
{
  return lj_ir_k64(J, IR_KNUM, lj_ir_k64_find(J, u64));
}

/* Intern 64 bit integer constant. */
TRef lj_ir_kint64(jit_State *J, uint64_t u64)
{
  return lj_ir_k64(J, IR_KINT64, lj_ir_k64_find(J, u64));
}

/* Check whether a number is int and return it. -0 is NOT considered an int. */
static int numistrueint(lua_Number n, int32_t *kp)
{
  int32_t k = lj_num2int(n);
  if (n == (lua_Number)k) {
    if (kp) *kp = k;
    if (k == 0) {  /* Special check for -0. */
      TValue tv;
      setnumV(&tv, n);
      if (tvismzero(&tv)) return 0;
    }
    return 1;
  }
  return 0;
}

/* Intern number as int32_t constant if possible, otherwise as FP constant. */
TRef lj_ir_knumint(jit_State *J, lua_Number n)
{
  int32_t k;
  if (numistrueint(n, &k))
    return lj_ir_kint(J, k);
  else
    return lj_ir_knum(J, n);
}

/* Intern GC object "constant". */
TRef lj_ir_kgc(jit_State *J, GCobj *o, IRType t)
{
  IRIns *ir, *cir = J->cur.ir;
  IRRef ref;
  lua_assert(!isdead(J2G(J), o) || uj_obj_is_sealed(o));
  for (ref = J->chain[IR_KGC]; ref; ref = cir[ref].prev)
    if (ir_kgc(&cir[ref]) == o)
      goto found;
  ref = ir_nextk(J);
  ir = IR(ref);
  /* NOBARRIER: Current trace is a GC root. */
  ir->ptr = o;
  ir->t.irt = (uint8_t)t;
  ir->o = IR_KGC;
  ir->prev = J->chain[IR_KGC];
  J->chain[IR_KGC] = (IRRef1)ref;
found:
  return TREF(ref, t);
}

/* Intern 32 bit pointer constant. */
TRef lj_ir_kptr_(jit_State *J, IROp op, void *ptr)
{
  IRIns *ir, *cir = J->cur.ir;
  IRRef ref;
  for (ref = J->chain[op]; ref; ref = cir[ref].prev)
    if (cir[ref].ptr == ptr)
      goto found;
  ref = ir_nextk(J);
  ir = IR(ref);
  ir->ptr = ptr;
  ir->t.irt = IRT_P32;
  ir->o = op;
  ir->prev = J->chain[op];
  J->chain[op] = (IRRef1)ref;
found:
  return TREF(ref, IRT_P32);
}

/* Intern typed NULL constant. */
TRef lj_ir_knull(jit_State *J, IRType t)
{
  IRIns *ir, *cir = J->cur.ir;
  IRRef ref;
  for (ref = J->chain[IR_KNULL]; ref; ref = cir[ref].prev)
    if (irt_t(cir[ref].t) == t)
      goto found;
  ref = ir_nextk(J);
  ir = IR(ref);
  ir->i = 0;
  ir->t.irt = (uint8_t)t;
  ir->o = IR_KNULL;
  ir->prev = J->chain[IR_KNULL];
  J->chain[IR_KNULL] = (IRRef1)ref;
found:
  return TREF(ref, t);
}

/* Intern key slot. */
TRef lj_ir_kslot(jit_State *J, TRef key, IRRef slot)
{
  IRIns *ir, *cir = J->cur.ir;
  IRRef2 op12 = IRREF2((IRRef1)key, (IRRef1)slot);
  IRRef ref;
  /* Const part is not touched by CSE/DCE, so 0-65535 is ok for IRMlit here. */
  lua_assert(tref_isk(key) && slot == (IRRef)(IRRef1)slot);
  for (ref = J->chain[IR_KSLOT]; ref; ref = cir[ref].prev)
    if (cir[ref].op12 == op12)
      goto found;
  ref = ir_nextk(J);
  ir = IR(ref);
  ir->op12 = op12;
  ir->t.irt = IRT_P32;
  ir->o = IR_KSLOT;
  ir->prev = J->chain[IR_KSLOT];
  J->chain[IR_KSLOT] = (IRRef1)ref;
found:
  return TREF(ref, IRT_P32);
}

/* -- Access to IR constants ---------------------------------------------- */

/* Copy value of IR constant. */
void lj_ir_kvalue(lua_State *L, TValue *tv, const IRIns *ir)
{
  UNUSED(L);
  lua_assert(ir->o != IR_KSLOT);  /* Common mistake. */
  switch (ir->o) {
  case IR_KPRI: settag(tv, irt_toitype(ir->t)); break;
  case IR_KINT: setintV(tv, ir->i); break;
  case IR_KGC: setgcV(L, tv, ir_kgc(ir), irt_toitype(ir->t)); break;
  case IR_KPTR: case IR_KKPTR: case IR_KNULL:
    setlightudV(tv, ir->ptr);
    break;
  case IR_KNUM: setnumV(tv, numV(ir_knum(ir))); break;
#if LJ_HASFFI
  case IR_KINT64: {
    GCcdata *cd = lj_cdata_new_(L, CTID_INT64, 8);
    *(uint64_t *)cdataptr(cd) = rawV(ir_kint64(ir));
    setcdataV(L, tv, cd);
    break;
    }
#endif
  default: lua_assert(0); break;
  }
}

/* -- Convert IR operand types -------------------------------------------- */

/* Convert from string to number. */
TRef lj_ir_tonumber(jit_State *J, TRef tr)
{
  if (!tref_isnumber(tr)) {
    if (tref_isstr(tr))
      tr = emitir(IRTG(IR_STRTO, IRT_NUM), tr, 0);
    else
      lj_trace_err(J, LJ_TRERR_BADTYPE);
  }
  return tr;
}

/* Convert from integer or string to number. */
TRef lj_ir_tonum(jit_State *J, TRef tr)
{
  if (!tref_isnum(tr)) {
    if (tref_isinteger(tr))
      tr = emitir(IRTN(IR_CONV), tr, IRCONV_NUM_INT);
    else if (tref_isstr(tr))
      tr = emitir(IRTG(IR_STRTO, IRT_NUM), tr, 0);
    else
      lj_trace_err(J, LJ_TRERR_BADTYPE);
  }
  return tr;
}

/* Convert from integer or number to string. */
TRef lj_ir_tostr(jit_State *J, TRef tr)
{
  if (!tref_isstr(tr)) {
    if (!tref_isnumber(tr))
      lj_trace_err(J, LJ_TRERR_BADTYPE);
    tr = emitir(IRT(IR_TOSTR, IRT_STR), tr, 0);
  }
  return tr;
}

/* -- Miscellaneous IR ops ------------------------------------------------ */

/* Evaluate numeric comparison. */
int lj_ir_numcmp(lua_Number a, lua_Number b, IROp op)
{
  switch (op) {
  case IR_EQ: return (a == b);
  case IR_NE: return (a != b);
  case IR_LT: return (a < b);
  case IR_GE: return (a >= b);
  case IR_LE: return (a <= b);
  case IR_GT: return (a > b);
  case IR_ULT: return !(a >= b);
  case IR_UGE: return !(a < b);
  case IR_ULE: return !(a > b);
  case IR_UGT: return !(a <= b);
  default: lua_assert(0); return 0;
  }
}

/* Evaluate string comparison. */
int lj_ir_strcmp(GCstr *a, GCstr *b, IROp op)
{
  int res = uj_str_cmp(a, b);
  switch (op) {
  case IR_LT: return (res < 0);
  case IR_GE: return (res >= 0);
  case IR_LE: return (res <= 0);
  case IR_GT: return (res > 0);
  default: lua_assert(0); return 0;
  }
}

/* Rollback IR to previous state. */
void lj_ir_rollback(jit_State *J, IRRef ref)
{
  IRRef nins = J->cur.nins;
  while (nins > ref) {
    IRIns *ir;
    nins--;
    ir = IR(nins);
    J->chain[ir->o] = ir->prev;
  }
  J->cur.nins = nins;
}

#undef IR
#undef fins
#undef emitir

#endif
