/*
 * Fast function call recorder.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "lj_obj.h"

#if LJ_HASJIT

#include "uj_dispatch.h"
#include "uj_throw.h"
#include "uj_str.h"
#include "lj_tab.h"
#include "lj_frame.h"
#include "uj_ff.h"
#include "jit/lj_ir.h"
#include "jit/lj_jit.h"
#include "jit/lj_ircall.h"
#include "jit/lj_iropt.h"
#include "jit/lj_trace.h"
#include "jit/lj_record.h"
#include "jit/lj_ffrecord.h"
#include "jit/uj_record_indexed.h"
#if LJ_HASFFI
#include "ffi/lj_crecord.h"
#endif
#include "lj_vm.h"

/* Some local macros to save typing. Undef'd at the end. */
#define IR(ref)                 (&J->cur.ir[(ref)])

/* Pass IR on to next optimization in chain (FOLD). */
#define emitir(ot, a, b)        (lj_ir_set(J, (ot), (a), (b)), lj_opt_fold(J))

/* -- Fast function recording handlers ------------------------------------ */

/* Conventions for fast function call handlers:
**
** The argument slots start at J->base[0]. All of them are guaranteed to be
** valid and type-specialized references. J->base[J->maxslot] is set to 0
** as a sentinel. The runtime argument values start at rd->argv[0].
**
** In general fast functions should check for presence of all of their
** arguments and for the correct argument types. Some simplifications
** are allowed if the interpreter throws instead. But even if recording
** is aborted, the generated IR must be consistent (no zero-refs).
**
** The number of results in rd->nres is set to 1. Handlers that return
** a different number of results need to override it. A negative value
** prevents return processing (e.g. for pending calls).
**
** Results need to be stored starting at J->base[0]. Return processing
** moves them to the right slots later.
**
** The per-ffid auxiliary data is the value of the 2nd part of the
** LJLIB_REC() annotation. This allows handling similar functionality
** in a common handler.
*/

/* Type of handler to record a fast function. */
typedef void (*RecordFunc)(jit_State *J, RecordFFData *rd);

/* Get runtime value of int argument. */
static int32_t argv2int(jit_State *J, TValue *o)
{
  if (!uj_str_tonumber(o))
    lj_trace_err(J, LJ_TRERR_BADTYPE);
  return lj_num2int(numV(o));
}

/* Get runtime value of string argument. */
static GCstr *argv2str(jit_State *J, TValue *o)
{
  if (LJ_LIKELY(tvisstr(o))) {
    return strV(o);
  } else {
    GCstr *s;
    if (!tvisnum(o))
      lj_trace_err(J, LJ_TRERR_BADTYPE);
    s = uj_str_fromnumber(J->L, o->n);
    setstrV(J->L, o, s);
    return s;
  }
}

/* Return number of results wanted by caller. */
static ptrdiff_t results_wanted(jit_State *J)
{
  TValue *frame = J->L->base-1;
  if (frame_islua(frame))
    return (ptrdiff_t)bc_b(frame_pc(frame)[-1]) - 1;
  else
    return -1;
}

/* Throw error for unsupported variant of fast function. */
LJ_NORET static void recff_nyiu(jit_State *J) {
  lj_trace_err_info_func(J, LJ_TRERR_NYIFFU);
}

/* Fallback handler for all fast functions that are not recorded (yet). */
LJ_NORET static void recff_nyi(jit_State *J, RecordFFData *rd) {
  UNUSED(rd); lj_trace_err_info_func(J, LJ_TRERR_NYIFF);
}

/* C functions can have arbitrary side-effects and are not recorded (yet). */
LJ_NORET static void recff_c(jit_State *J, RecordFFData *rd) {
  UNUSED(rd); lj_trace_err_info_func(J, LJ_TRERR_NYICF);
}

/* Emit BUFHDR for the global temporary buffer. */
static TRef recff_bufhdr(jit_State *J)
{
  return emitir(IRT(IR_BUFHDR, IRT_PTR),
                lj_ir_kptr(J, &J2G(J)->tmpbuf), IRBUFHDR_RESET);
}

/* -- Base library fast functions ----------------------------------------- */

static void recff_assert(jit_State *J, RecordFFData *rd)
{
  /* Arguments already specialized. The interpreter throws for nil/false. */
  rd->nres = J->maxslot;  /* Pass through all arguments. */
}

static void recff_type(jit_State *J, RecordFFData *rd)
{
  /* Arguments already specialized. Result is a constant string. Neat, huh? */
  uint32_t t = ~gettag(&rd->argv[0]);
  J->base[0] = lj_ir_kstr(J, strV(&J->fn->c.upvalue[t]));
  UNUSED(rd);
}

static void recff_getmetatable(jit_State *J, RecordFFData *rd)
{
  TRef tr = J->base[0];
  if (tr) {
    RecordIndex ix;
    ix.tab = tr;
    copyTV(J->L, &ix.tabv, &rd->argv[0]);
    if (lj_record_mm_lookup(J, &ix, MM_metatable))
      J->base[0] = ix.mobj;
    else
      J->base[0] = ix.mt;
  }  /* else: Interpreter will throw. */
}

static void recff_setmetatable(jit_State *J, RecordFFData *rd)
{
  TRef tr = J->base[0];
  TRef mt = J->base[1];
  if (tref_istab(tr) && (tref_istab(mt) || (mt && tref_isnil(mt)))) {
    TRef fref, mtref;
    RecordIndex ix;
    ix.tab = tr;
    lj_ir_emit_immutable_guard(J, tr);
    copyTV(J->L, &ix.tabv, &rd->argv[0]);
    lj_record_mm_lookup(J, &ix, MM_metatable); /* Guard for no __metatable. */
    fref = emitir(IRT(IR_FREF, IRT_P32), tr, IRFL_TAB_META);
    mtref = tref_isnil(mt) ? lj_ir_knull(J, IRT_TAB) : mt;
    emitir(IRT(IR_FSTORE, IRT_TAB), fref, mtref);
    if (!tref_isnil(mt)) {
      emitir(IRT(IR_TBAR, IRT_TAB), tr, 0);
      if (!tref_isk(tr))
        ir_sethint(IR(tref_ref(tr)), IRH_TAB_SETMETA);
    }
    J->base[0] = tr;
    J->needsnap = 1;
  }  /* else: Interpreter will throw. */
}

static void recff_rawget(jit_State *J, RecordFFData *rd)
{
  RecordIndex ix;
  ix.tab = J->base[0]; ix.key = J->base[1];
  if (tref_istab(ix.tab) && ix.key) {
    ix.val = 0; ix.idxchain = 0;
    settabV(J->L, &ix.tabv, tabV(&rd->argv[0]));
    copyTV(J->L, &ix.keyv, &rd->argv[1]);
    J->base[0] = uj_record_indexed(J, &ix);
  }  /* else: Interpreter will throw. */
}

static void recff_rawset(jit_State *J, RecordFFData *rd)
{
  RecordIndex ix;
  ix.tab = J->base[0]; ix.key = J->base[1]; ix.val = J->base[2];
  if (tref_istab(ix.tab) && ix.key && ix.val) {
    ix.idxchain = 0;
    settabV(J->L, &ix.tabv, tabV(&rd->argv[0]));
    copyTV(J->L, &ix.keyv, &rd->argv[1]);
    copyTV(J->L, &ix.valv, &rd->argv[2]);
    uj_record_indexed(J, &ix);
    /* Pass through table at J->base[0] as result. */
  }  /* else: Interpreter will throw. */
}

static void recff_rawequal(jit_State *J, RecordFFData *rd)
{
  TRef tra = J->base[0];
  TRef trb = J->base[1];
  if (tra && trb) {
    int diff = lj_record_objcmp(J, tra, trb, &rd->argv[0], &rd->argv[1]);
    J->base[0] = diff ? TREF_FALSE : TREF_TRUE;
  }  /* else: Interpreter will throw. */
}

#if LJ_52
static void recff_rawlen(jit_State *J, RecordFFData *rd)
{
  TRef tr = J->base[0];
  if (tref_isstr(tr))
    J->base[0] = emitir(IRTI(IR_FLOAD), tr, IRFL_STR_LEN);
  else if (tref_istab(tr))
    J->base[0] = lj_ir_call(J, IRCALL_lj_tab_len, tr);
  /* else: Interpreter will throw. */
  UNUSED(rd);
}
#endif

/* Determine mode of select() call. */
int32_t lj_ffrecord_select_mode(jit_State *J, TRef tr, TValue *tv)
{
  if (tref_isstr(tr) && *strVdata(tv) == '#') {  /* select('#', ...) */
    if (strV(tv)->len == 1) {
      emitir(IRTG(IR_EQ, IRT_STR), tr, lj_ir_kstr(J, strV(tv)));
    } else {
      TRef trptr = emitir(IRT(IR_STRREF, IRT_P32), tr, lj_ir_kint(J, 0));
      TRef trchar = emitir(IRT(IR_XLOAD, IRT_U8), trptr, IRXLOAD_READONLY);
      emitir(IRTG(IR_EQ, IRT_INT), trchar, lj_ir_kint(J, '#'));
    }
    return 0;
  } else {  /* select(n, ...) */
    int32_t start = argv2int(J, tv);
    if (start == 0) lj_trace_err(J, LJ_TRERR_BADTYPE);  /* A bit misleading. */
    return start;
  }
}

static void recff_select(jit_State *J, RecordFFData *rd)
{
  TRef tr = J->base[0];
  if (tr) {
    ptrdiff_t start = lj_ffrecord_select_mode(J, tr, &rd->argv[0]);
    if (start == 0) {  /* select('#', ...) */
      J->base[0] = lj_ir_kint(J, J->maxslot - 1);
    } else if (tref_isk(tr)) {  /* select(k, ...) */
      ptrdiff_t n = (ptrdiff_t)J->maxslot;
      if (start < 0) start += n;
      else if (start > n) start = n;
      rd->nres = n - start;
      if (start >= 1) {
        ptrdiff_t i;
        for (i = 0; i < n - start; i++)
          J->base[i] = J->base[start+i];
      }  /* else: Interpreter will throw. */
    } else {
      recff_nyiu(J);
    }
  }  /* else: Interpreter will throw. */
}

static void recff_tonumber(jit_State *J, RecordFFData *rd)
{
  TRef tr = J->base[0];
  TRef base = J->base[1];
  if (tr && !tref_isnil(base)) {
    base = lj_opt_narrow_toint(J, base);
    if (!tref_isk(base) || IR(tref_ref(base))->i != 10)
      recff_nyiu(J);
  }
  if (tref_isnumber_str(tr)) {
    if (tref_isstr(tr)) {
      TValue tmp;
      if (!uj_str_tonumtv(strV(&rd->argv[0]), &tmp))
        recff_nyiu(J);  /* Would need an inverted STRTO for this case. */
      tr = emitir(IRTG(IR_STRTO, IRT_NUM), tr, 0);
    }
#if LJ_HASFFI
  } else if (tref_iscdata(tr)) {
    lj_crecord_tonumber(J, rd);
    return;
#endif
  } else {
    tr = TREF_NIL;
  }
  J->base[0] = tr;
  UNUSED(rd);
}

static TValue *recff_metacall_cp(lua_State *L, lua_CFunction dummy, void *ud)
{
  jit_State *J = (jit_State *)ud;
  lj_record_tailcall(J, 0, 1);
  UNUSED(L); UNUSED(dummy);
  return NULL;
}

static int recff_metacall(jit_State *J, RecordFFData *rd, enum MMS mm)
{
  RecordIndex ix;
  ix.tab = J->base[0];
  copyTV(J->L, &ix.tabv, &rd->argv[0]);
  if (lj_record_mm_lookup(J, &ix, mm)) {  /* Has metamethod? */
    int errcode;
    TValue argv0;
    /* Temporarily insert metamethod below object. */
    J->base[1] = J->base[0];
    J->base[0] = ix.mobj;
    copyTV(J->L, &argv0, &rd->argv[0]);
    copyTV(J->L, &rd->argv[1], &rd->argv[0]);
    copyTV(J->L, &rd->argv[0], &ix.mobjv);
    /* Need to protect lj_record_tailcall because it may throw. */
    errcode = lj_vm_cpcall(J->L, NULL, J, recff_metacall_cp);
    /* Always undo Lua stack changes to avoid confusing the interpreter. */
    copyTV(J->L, &rd->argv[0], &argv0);
    if (errcode)
      uj_throw(J->L, errcode);  /* Propagate errors. */
    rd->nres = -1;  /* Pending call. */
    return 1;  /* Tailcalled to metamethod. */
  }
  return 0;
}

static void recff_tostring(jit_State *J, RecordFFData *rd)
{
  TRef tr = J->base[0];
  if (tref_isstr(tr)) {
    /* Ignore __tostring in the string base metatable. */
    /* Pass on result in J->base[0]. */
  } else if (!recff_metacall(J, rd, MM_tostring)) {
    if (tref_isnumber(tr)) {
      J->base[0] = emitir(IRT(IR_TOSTR, IRT_STR), tr, 0);
    } else if (tref_ispri(tr)) {
      J->base[0] = lj_ir_kstr(J, strV(&J->fn->c.upvalue[tref_type(tr)]));
    } else {
      recff_nyiu(J);
    }
  }
}

static void recff_ipairs_aux(jit_State *J, RecordFFData *rd)
{
  RecordIndex ix;
  ix.tab = J->base[0];
  if (tref_istab(ix.tab)) {
    if (!tvisnum(&rd->argv[1]))  /* No support for string coercion. */
      lj_trace_err(J, LJ_TRERR_BADTYPE);
    setintV(&ix.keyv, lj_num2int(numV(&rd->argv[1]))+1);
    settabV(J->L, &ix.tabv, tabV(&rd->argv[0]));
    ix.val = 0; ix.idxchain = 0;
    ix.key = lj_opt_narrow_toint(J, J->base[1]);
    J->base[0] = ix.key = emitir(IRTI(IR_ADD), ix.key, lj_ir_kint(J, 1));
    J->base[1] = uj_record_indexed(J, &ix);
    rd->nres = tref_isnil(J->base[1]) ? 0 : 2;
  }  /* else: Interpreter will throw. */
}

static void recff_xpairs(jit_State *J, RecordFFData *rd, enum MMS mm, TRef init)
{
  TRef tr = J->base[0];
  if (!((LJ_52 || (LJ_HASFFI && tref_iscdata(tr))) &&
   recff_metacall(J, rd, mm))) {
    if (tref_istab(tr)) {
      J->base[0] = lj_ir_kfunc(J, funcV(&J->fn->c.upvalue[0]));
      J->base[1] = tr;
      J->base[2] = init;
      rd->nres = 3;
    }  /* else: Interpreter will throw. */
  }
}

static void recff_ipairs(jit_State *J, RecordFFData *rd)
{
  recff_xpairs(J, rd, MM_ipairs, lj_ir_kint(J, 0));
}

/*
 * In order to use lj_tab_next() in next() recording
 * be sure that keyv and valv are adjacent in RecordIndex
 */
LJ_STATIC_ASSERT(
  (offsetof(RecordIndex, valv) - offsetof(RecordIndex, keyv)) == sizeof(TValue)
);

static LJ_AINLINE int recff_in_array(const GCtab *t, const TValue *key)
{
  lua_Number nk;
  int32_t ik;

  if (!tvisnum(key))
    return 0;

  nk = numV(key);
  ik = lj_num2int(nk);
  if (nk != ik)
    return 0;

  return inarray(t, ik);
}

static void recff_next(jit_State *J, RecordFFData *rd)
{
  RecordIndex ix;

  if (!(J->flags & JIT_F_OPT_JITPAIRS))
    recff_nyi(J, rd);

  ix.tab = J->base[0];
  ix.key = J->base[1];
  if (tref_istab(ix.tab)) {
    const GCtab *t;
    int key_in_array, next_key_in_array;

    copyTV(J->L, &ix.tabv, &rd->argv[0]);
    copyTV(J->L, &ix.keyv, &rd->argv[1]);
    ix.val = 0;
    ix.idxchain = 0;
    t = tabV(&ix.tabv);
    /* Assume that nil key is in array under -1 index */
    key_in_array = recff_in_array(t, &ix.keyv) || tref_isnil(ix.key);

    if (!lj_tab_next(J->L, t, &ix.keyv)) /* Updates ix.keyv and ix.valv */
      recff_nyiu(J); /* Let's not record a trace without iterations */

    next_key_in_array = recff_in_array(t, &ix.keyv);

    if (key_in_array && next_key_in_array) {
      TRef ikey = tref_isnil(ix.key) ? lj_ir_kint(J, ~0u)
                                     : lj_opt_narrow_index(J, ix.key);
      ix.key = lj_ir_call(J, IRCALL_lj_tab_nexta, ix.tab, ikey);
      ix.val = uj_record_indexed(J, &ix);
    } else if (key_in_array && !next_key_in_array) {
      /*
       * Cannot record a trace, because we don't have '-1' node for hash
       * or in other words HREF IR requires existing key as an operand.
       */
      recff_nyiu(J);
    } else if (!key_in_array && next_key_in_array) {
      lua_assert(0); /* Something is really wrong */
    } else { /* Keys in hash part */
      TRef noderef = emitir(IRT(IR_HREF, IRT_PTR), ix.tab, ix.key);
      TRef nextnode = lj_ir_call(J, IRCALL_lj_tab_nexth, ix.tab, noderef);
      ix.key = emitir(IRTG(IR_HKLOAD, itype2irt(&ix.keyv)), nextnode, 0);
      ix.val = emitir(IRTG(IR_HLOAD, itype2irt(&ix.valv)), nextnode, 0);
    }
    J->base[0] = ix.key;
    J->base[1] = ix.val;
    rd->nres = 2;
  }  /* else: Interpreter will throw. */
}

static void recff_pairs(jit_State *J, RecordFFData *rd)
{
  if (!(J->flags & JIT_F_OPT_JITPAIRS))
    recff_nyi(J, rd);
  recff_xpairs(J, rd, MM_pairs, TREF_NIL);
}

static void recff_pcall(jit_State *J, RecordFFData *rd)
{
  if (J->maxslot >= 1) {
    lj_record_call(J, 0, J->maxslot - 1);
    rd->nres = -1;  /* Pending call. */
  }  /* else: Interpreter will throw. */
}

static TValue *recff_xpcall_cp(lua_State *L, lua_CFunction dummy, void *ud)
{
  jit_State *J = (jit_State *)ud;
  lj_record_call(J, 1, J->maxslot - 2);
  UNUSED(L); UNUSED(dummy);
  return NULL;
}

static void recff_xpcall(jit_State *J, RecordFFData *rd)
{
  if (J->maxslot >= 2) {
    TValue argv0, argv1;
    TRef tmp;
    int errcode;
    /* Swap function and traceback. */
    tmp = J->base[0]; J->base[0] = J->base[1]; J->base[1] = tmp;
    copyTV(J->L, &argv0, &rd->argv[0]);
    copyTV(J->L, &argv1, &rd->argv[1]);
    copyTV(J->L, &rd->argv[0], &argv1);
    copyTV(J->L, &rd->argv[1], &argv0);
    /* Need to protect lj_record_call because it may throw. */
    errcode = lj_vm_cpcall(J->L, NULL, J, recff_xpcall_cp);
    /* Always undo Lua stack swap to avoid confusing the interpreter. */
    copyTV(J->L, &rd->argv[0], &argv0);
    copyTV(J->L, &rd->argv[1], &argv1);
    if (errcode)
      uj_throw(J->L, errcode);  /* Propagate errors. */
    rd->nres = -1;  /* Pending call. */
  }  /* else: Interpreter will throw. */
}

static void recff_getfenv(jit_State *J, RecordFFData *rd) {
  TRef tr = J->base[0];
  /* Only support getfenv(0) for now. */
  if (tref_isint(tr) && tref_isk(tr) && IR(tref_ref(tr))->i == 0) {
    TRef trl = emitir(IRT(IR_LREF, IRT_THREAD), 0, 0);
    J->base[0] = emitir(IRT(IR_FLOAD, IRT_TAB), trl, IRFL_THREAD_ENV);
    return;
  }
  recff_nyiu(J);
  UNUSED(rd);
}

/* -- Math library fast functions ----------------------------------------- */

static void recff_math_abs(jit_State *J, RecordFFData *rd)
{
  TRef tr = lj_ir_tonum(J, J->base[0]);
  J->base[0] = emitir(IRTN(IR_ABS), tr, lj_ir_knum_abs(J));
  UNUSED(rd);
}

/* Record rounding functions math.floor and math.ceil. */
static void recff_helper_math_round(jit_State *J, IRFPMathOp op)
{
  TRef tr = J->base[0];
  if (!tref_isinteger(tr)) {  /* Pass through integers unmodified. */
    tr = emitir(IRTN(IR_FPMATH), lj_ir_tonum(J, tr), op);
    /* Result is integral (or NaN/Inf), but may not fit an int32_t. */
    J->base[0] = tr;
  }
}

static void recff_math_floor(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);
  recff_helper_math_round(J, IRFPM_FLOOR);
}

static void recff_math_ceil(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);
  recff_helper_math_round(J, IRFPM_CEIL);
}

/* Record unary math.* functions, mapped to IR_FPMATH opcode. */
static void recff_helper_math_unary(jit_State *J, IRFPMathOp op)
{
  J->base[0] = emitir(IRTN(IR_FPMATH), lj_ir_tonum(J, J->base[0]), op);
}

static void recff_math_sqrt(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);
  recff_helper_math_unary(J, IRFPM_SQRT);
}

static void recff_math_log10(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);
  recff_helper_math_unary(J, IRFPM_LOG10);
}

static void recff_math_exp(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);
  recff_helper_math_unary(J, IRFPM_EXP);
}

static void recff_math_sin(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);
  recff_helper_math_unary(J, IRFPM_SIN);
}

static void recff_math_cos(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);
  recff_helper_math_unary(J, IRFPM_COS);
}

static void recff_math_tan(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);
  recff_helper_math_unary(J, IRFPM_TAN);
}

/* Record math.log. */
static void recff_math_log(jit_State *J, RecordFFData *rd)
{
  TRef tr = lj_ir_tonum(J, J->base[0]);
  if (J->base[1]) {
    uint32_t fpm = IRFPM_LOG2;
    TRef trb = lj_ir_tonum(J, J->base[1]);
    tr = emitir(IRTN(IR_FPMATH), tr, fpm);
    trb = emitir(IRTN(IR_FPMATH), trb, fpm);
    trb = emitir(IRTN(IR_DIV), lj_ir_knum_one(J), trb);
    tr = emitir(IRTN(IR_MUL), tr, trb);
  } else {
    tr = emitir(IRTN(IR_FPMATH), tr, IRFPM_LOG);
  }
  J->base[0] = tr;
  UNUSED(rd);
}

/* Record math.atan2. */
static void recff_math_atan2(jit_State *J, RecordFFData *rd)
{
  TRef y = lj_ir_tonum(J, J->base[0]);
  TRef x = lj_ir_tonum(J, J->base[1]);

  J->base[0] = emitir(IRTN(IR_ATAN2), y, x);
  UNUSED(rd);
}

/* Record math.ldexp. */
static void recff_math_ldexp(jit_State *J, RecordFFData *rd)
{
  TRef tr = lj_ir_tonum(J, J->base[0]);
  TRef tr2 = lj_ir_tonum(J, J->base[1]);
  J->base[0] = emitir(IRTN(IR_LDEXP), tr, tr2);
  UNUSED(rd);
}

static void recff_helper_math_degrad(jit_State *J)
{
  TRef tr = lj_ir_tonum(J, J->base[0]);
  TRef trm = lj_ir_knum(J, numV(&J->fn->c.upvalue[0]));
  J->base[0] = emitir(IRTN(IR_MUL), tr, trm);
}

static void recff_math_deg(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);
  recff_helper_math_degrad(J);
}

static void recff_math_rad(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);
  recff_helper_math_degrad(J);
}

static void recff_math_pow(jit_State *J, RecordFFData *rd)
{
  J->base[0] = lj_opt_narrow_pow(J, J->base[0], J->base[1],
                                 &rd->argv[0], &rd->argv[1]);
  UNUSED(rd);
}

static void recff_helper_math_minmax(jit_State *J, IROp op)
{
  TRef tr = lj_ir_tonumber(J, J->base[0]);
  BCReg i;
  for (i = 1; J->base[i] != 0; i++) {
    TRef tr2 = lj_ir_tonumber(J, J->base[i]);
    IRType t = IRT_INT;
    if (!(tref_isinteger(tr) && tref_isinteger(tr2))) {
      if (tref_isinteger(tr)) tr = emitir(IRTN(IR_CONV), tr, IRCONV_NUM_INT);
      if (tref_isinteger(tr2)) tr2 = emitir(IRTN(IR_CONV), tr2, IRCONV_NUM_INT);
      t = IRT_NUM;
    }
    tr = emitir(IRT(op, t), tr, tr2);
  }
  J->base[0] = tr;
}

static void recff_math_min(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);
  recff_helper_math_minmax(J, IR_MIN);
}

static void recff_math_max(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);
  recff_helper_math_minmax(J, IR_MAX);
}

static void recff_math_random(jit_State *J, RecordFFData *rd)
{
  GCudata *ud = udataV(&J->fn->c.upvalue[0]);
  TRef tr, one;
  lj_ir_kgc(J, obj2gco(ud), IRT_UDATA);  /* Prevent collection. */
  tr = lj_ir_call(J, IRCALL_lj_math_random_step, lj_ir_kptr(J, uddata(ud)));
  one = lj_ir_knum_one(J);
  tr = emitir(IRTN(IR_SUB), tr, one);
  if (J->base[0]) {
    TRef tr1 = lj_ir_tonum(J, J->base[0]);
    if (J->base[1]) {  /* d = floor(d*(r2-r1+1.0)) + r1 */
      TRef tr2 = lj_ir_tonum(J, J->base[1]);
      tr2 = emitir(IRTN(IR_SUB), tr2, tr1);
      tr2 = emitir(IRTN(IR_ADD), tr2, one);
      tr = emitir(IRTN(IR_MUL), tr, tr2);
      tr = emitir(IRTN(IR_FPMATH), tr, IRFPM_FLOOR);
      tr = emitir(IRTN(IR_ADD), tr, tr1);
    } else {  /* d = floor(d*r1) + 1.0 */
      tr = emitir(IRTN(IR_MUL), tr, tr1);
      tr = emitir(IRTN(IR_FPMATH), tr, IRFPM_FLOOR);
      tr = emitir(IRTN(IR_ADD), tr, one);
    }
  }
  J->base[0] = tr;
  UNUSED(rd);
}

static void recff_math_asin(jit_State *J, RecordFFData *rd)
{
  TRef x = lj_ir_tonum(J, J->base[0]);

  /* y = sqrt(1 - x^2) */
  TRef tr = emitir(IRTN(IR_MUL), x, x);
  tr = emitir(IRTN(IR_SUB), lj_ir_knum_one(J), tr);
  tr = emitir(IRTN(IR_FPMATH), tr, IRFPM_SQRT);

  /* asin(x) = atan2(x, y) */
  J->base[0] = emitir(IRTN(IR_ATAN2), x, tr);
  UNUSED(rd);
}

static void recff_math_acos(jit_State *J, RecordFFData *rd)
{
  TRef x = lj_ir_tonum(J, J->base[0]);

  /* y = sqrt(1 - x^2) */
  TRef tr = emitir(IRTN(IR_MUL), x, x);
  tr = emitir(IRTN(IR_SUB), lj_ir_knum_one(J), tr);
  tr = emitir(IRTN(IR_FPMATH), tr, IRFPM_SQRT);

  /* acos(x) = atan2(y, x) */
  J->base[0] = emitir(IRTN(IR_ATAN2), tr, x);
  UNUSED(rd);
}

static void recff_math_atan(jit_State *J, RecordFFData *rd)
{
  TRef x = lj_ir_tonum(J, J->base[0]);

  J->base[0] = emitir(IRTN(IR_ATAN2), x, lj_ir_knum_one(J));
  UNUSED(rd);
}

static void recff_math_sinh(jit_State *J, RecordFFData *rd)
{
    TRef x = lj_ir_tonum(J, J->base[0]);

    J->base[0] = lj_ir_call(J, IRCALL_sinh, x);
    UNUSED(rd);
}

static void recff_math_cosh(jit_State *J, RecordFFData *rd)
{
    TRef x = lj_ir_tonum(J, J->base[0]);

    J->base[0] = lj_ir_call(J, IRCALL_cosh, x);
    UNUSED(rd);
}

static void recff_math_tanh(jit_State *J, RecordFFData *rd)
{
    TRef x = lj_ir_tonum(J, J->base[0]);

    J->base[0] = lj_ir_call(J, IRCALL_tanh, x);
    UNUSED(rd);
}

/* -- Bit library fast functions ------------------------------------------ */

/* Record unary bit.tobit, bit.bnot, bit.bswap. */
static void recff_helper_bit_unary(jit_State *J, IROp op)
{
  TRef tr = lj_opt_narrow_tobit(J, J->base[0]);
  J->base[0] = (op == IR_TOBIT) ? tr : emitir(IRTI(op), tr, 0);
}

static void recff_bit_tobit(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);
  recff_helper_bit_unary(J, IR_TOBIT);
}

static void recff_bit_bnot(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);
  recff_helper_bit_unary(J, IR_BNOT);
}

static void recff_bit_bswap(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);
  recff_helper_bit_unary(J, IR_BSWAP);
}

/* Record N-ary bit.band, bit.bor, bit.bxor. */
static void recff_helper_bit_nary(jit_State *J, IROp op)
{
  TRef tr = lj_opt_narrow_tobit(J, J->base[0]);
  BCReg i;
  for (i = 1; J->base[i] != 0; i++)
    tr = emitir(IRTI(op), tr, lj_opt_narrow_tobit(J, J->base[i]));
  J->base[0] = tr;
}

static void recff_bit_band(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);
  recff_helper_bit_nary(J, IR_BAND);
}

static void recff_bit_bor(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);
  recff_helper_bit_nary(J, IR_BOR);
}

static void recff_bit_bxor(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);
  recff_helper_bit_nary(J, IR_BXOR);
}

/* Record bit shifts. */
static void recff_helper_bit_shift(jit_State *J, IROp op)
{
  TRef tr = lj_opt_narrow_tobit(J, J->base[0]);
  TRef tsh = lj_opt_narrow_tobit(J, J->base[1]);
  if (!(op < IR_BROL ? UJ_TARGET_MASKSHIFT : UJ_TARGET_MASKROT) &&
      !tref_isk(tsh))
    tsh = emitir(IRTI(IR_BAND), tsh, lj_ir_kint(J, 31));
#ifdef UJ_TARGET_UNIFYROT
  if (op == (UJ_TARGET_UNIFYROT == 1 ? IR_BROR : IR_BROL)) {
    op = UJ_TARGET_UNIFYROT == 1 ? IR_BROL : IR_BROR;
    tsh = emitir(IRTI(IR_NEG), tsh, tsh);
  }
#endif
  J->base[0] = emitir(IRTI(op), tr, tsh);
}

static void recff_bit_lshift(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);
  recff_helper_bit_shift(J, IR_BSHL);
}

static void recff_bit_rshift(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);
  recff_helper_bit_shift(J, IR_BSHR);
}

static void recff_bit_arshift(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);
  recff_helper_bit_shift(J, IR_BSAR);
}

static void recff_bit_rol(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);
  recff_helper_bit_shift(J, IR_BROL);
}

static void recff_bit_ror(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);
  recff_helper_bit_shift(J, IR_BROR);
}

/* -- String library fast functions --------------------------------------- */

/*
 * Specialize to relative starting position for string.
 * Return a reference to C-style start index.
 */
static TRef recff_string_start(jit_State *J, int32_t *start_ptr, size_t len,
                               TRef trstart, TRef trlen)
{
  TRef tr0 = lj_ir_kint(J, 0);
  int32_t start = *start_ptr;
  if (start < 0) {
    emitir(IRTGI(IR_LT), trstart, tr0);
    trstart = emitir(IRTI(IR_ADD), trlen, trstart);
    start = start + (int32_t)len;
    emitir(start < 0 ? IRTGI(IR_LT) : IRTGI(IR_GE), trstart, tr0);
    if (start < 0) {
      trstart = tr0;
      start = 0;
    }
  } else if (start == 0) {
    emitir(IRTGI(IR_EQ), trstart, tr0);
    trstart = tr0;
  } else {
    trstart = emitir(IRTI(IR_ADD), trstart, lj_ir_kint(J, -1));
    emitir(IRTGI(IR_GE), trstart, tr0);
    start--;
  }
  *start_ptr = start;
  return trstart;
}

static void recff_string_len(jit_State *J, RecordFFData *rd)
{
  J->base[0] = emitir(IRTI(IR_FLOAD), lj_ir_tostr(J, J->base[0]), IRFL_STR_LEN);
  UNUSED(rd);
}

/* Handle string.byte (is_string_sub = 0) and string.sub (is_string_sub = 1). */
static void recff_helper_string_range(jit_State *J, RecordFFData *rd, int is_string_sub)
{
  TRef trstr = lj_ir_tostr(J, J->base[0]);
  TRef trlen = emitir(IRTI(IR_FLOAD), trstr, IRFL_STR_LEN);
  TRef tr0 = lj_ir_kint(J, 0);
  TRef trstart, trend;
  GCstr *str = argv2str(J, &rd->argv[0]);
  int32_t start, end;
  if (is_string_sub) {  /* string.sub(str, start [,end]) */
    start = argv2int(J, &rd->argv[1]);
    trstart = lj_opt_narrow_toint(J, J->base[1]);
    trend = J->base[2];
    if (tref_isnil(trend)) {
      trend = lj_ir_kint(J, -1);
      end = -1;
    } else {
      trend = lj_opt_narrow_toint(J, trend);
      end = argv2int(J, &rd->argv[2]);
    }
  } else {  /* string.byte(str, [,start [,end]]) */
    if (tref_isnil(J->base[1])) {
      start = 1;
      trstart = lj_ir_kint(J, 1);
    } else {
      start = argv2int(J, &rd->argv[1]);
      trstart = lj_opt_narrow_toint(J, J->base[1]);
    }
    if (J->base[1] && !tref_isnil(J->base[2])) {
      trend = lj_opt_narrow_toint(J, J->base[2]);
      end = argv2int(J, &rd->argv[2]);
    } else {
      trend = trstart;
      end = start;
    }
  }
  if (end < 0) {
    emitir(IRTGI(IR_LT), trend, tr0);
    trend = emitir(IRTI(IR_ADD), emitir(IRTI(IR_ADD), trlen, trend),
                   lj_ir_kint(J, 1));
    end = end+(int32_t)str->len+1;
  } else if ((size_t)end <= str->len) {
    emitir(IRTGI(IR_ULE), trend, trlen);
  } else {
    emitir(IRTGI(IR_GT), trend, trlen);
    end = (int32_t)str->len;
    trend = trlen;
  }
  trstart = recff_string_start(J, &start, str->len, trstart, trlen);
  if (is_string_sub) {  /* Return string.sub result. */
    if (end - start >= 0) {
      /* Also handle empty range here, to avoid extra traces. */
      TRef trptr, trslen = emitir(IRTI(IR_SUB), trend, trstart);
      emitir(IRTGI(IR_GE), trslen, tr0);
      trptr = emitir(IRT(IR_STRREF, IRT_P32), trstr, trstart);
      J->base[0] = emitir(IRT(IR_SNEW, IRT_STR), trptr, trslen);
    } else {  /* Range underflow: return empty string. */
      emitir(IRTGI(IR_LT), trend, trstart);
      J->base[0] = lj_ir_kstr(J, uj_str_new(J->L, strdata(str), 0));
    }
  } else {  /* Return string.byte result(s). */
    ptrdiff_t i, len = end - start;
    if (len > 0) {
      TRef trslen = emitir(IRTI(IR_SUB), trend, trstart);
      emitir(IRTGI(IR_EQ), trslen, lj_ir_kint(J, (int32_t)len));
      if (J->baseslot + len > LJ_MAX_JSLOTS) {
        lj_trace_err(J, LJ_TRERR_STACKOV);
      }
      rd->nres = len;
      for (i = 0; i < len; i++) {
        TRef tmp = emitir(IRTI(IR_ADD), trstart, lj_ir_kint(J, (int32_t)i));
        tmp = emitir(IRT(IR_STRREF, IRT_P32), trstr, tmp);
        J->base[i] = emitir(IRT(IR_XLOAD, IRT_U8), tmp, IRXLOAD_READONLY);
      }
    } else {  /* Empty range or range underflow: return no results. */
      emitir(IRTGI(IR_LE), trend, trstart);
      rd->nres = 0;
    }
  }
}

static void recff_string_byte(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);
  recff_helper_string_range(J, rd, 0);
}

static void recff_string_sub(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);
  recff_helper_string_range(J, rd, 1);
}

static LJ_AINLINE void recff_string_op(jit_State *J, RecordFFData *rd,
                                       IRCallID id)
{
  TRef str;

  if (!(J->flags & JIT_F_OPT_JITSTR))
    recff_nyi(J, rd);

  str = lj_ir_tostr(J, J->base[0]);
  J->base[0] = lj_ir_call(J, id, str);
}

static void recff_string_lower(jit_State *J, RecordFFData *rd)
{
  recff_string_op(J, rd, IRCALL_uj_str_lower);
}

static void recff_string_upper(jit_State *J, RecordFFData *rd)
{
  recff_string_op(J, rd, IRCALL_uj_str_upper);
}

static void recff_string_find(jit_State *J, RecordFFData *rd)
{
  TRef trstr, trpat, trlen, tr0, trstart;
  GCstr *str, *pat;
  int32_t start;
  int plain_arg;

  if (!(J->flags & JIT_F_OPT_JITSTR))
    recff_nyi(J, rd);

  trstr = lj_ir_tostr(J, J->base[0]);
  trpat = lj_ir_tostr(J, J->base[1]);
  trlen = emitir(IRTI(IR_FLOAD), trstr, IRFL_STR_LEN);
  tr0 = lj_ir_kint(J, 0);
  str = argv2str(J, &rd->argv[0]);
  pat = argv2str(J, &rd->argv[1]);
  if (tref_isnil(J->base[2])) {
    trstart = lj_ir_kint(J, 1);
    start = 1;
  } else {
    trstart = lj_opt_narrow_toint(J, J->base[2]);
    start = argv2int(J, &rd->argv[2]);
  }
  trstart = recff_string_start(J, &start, str->len, trstart, trlen);
  if ((size_t)start <= str->len) {
    emitir(IRTGI(IR_ULE), trstart, trlen);
  } else {
    emitir(IRTGI(IR_UGT), trstart, trlen);
#if LJ_52
    J->base[0] = TREF_NIL;
    return;
#else
    trstart = trlen;
    start = str->len;
#endif
  }
  /* 'plain' arg or no pattern matching chars? */
  plain_arg = J->base[2] && tref_istruecond(J->base[3]);
  if (plain_arg || !uj_str_has_pattern_specials(pat)) {  /* Plain search */
    TRef trsptr, trpptr, trslen, trplen, trfindptr, trnullptr;
    if (!plain_arg) {
      TRef haspattern = lj_ir_call(J, IRCALL_uj_str_has_pattern_specials, trpat);
      emitir(IRTGI(IR_EQ), haspattern, tr0);
    }
    trsptr = emitir(IRT(IR_STRREF, IRT_PTR), trstr, trstart);
    trpptr = emitir(IRT(IR_STRREF, IRT_PTR), trpat, tr0);
    trslen = emitir(IRTI(IR_SUB), trlen, trstart);
    trplen = emitir(IRTI(IR_FLOAD), trpat, IRFL_STR_LEN);
    trfindptr = lj_ir_call(J, IRCALL_uj_cstr_find, trsptr, trpptr, trslen, trplen);
    trnullptr = lj_ir_kkptr(J, NULL);
    if (uj_cstr_find(strdata(str) + (size_t)start, strdata(pat),
                     str->len - (size_t)start, pat->len)) {
      TRef pos;
      emitir(IRTG(IR_NE, IRT_PTR), trfindptr, trnullptr);
      /*
       * Caveat: can't use STRREF trstr 0 here because that might be pointing
       * into a wrong string due to folding.
       */
      pos = emitir(IRTI(IR_ADD), trstart,
                   emitir(IRTI(IR_SUB), trfindptr, trsptr));
      J->base[0] = emitir(IRTI(IR_ADD), pos, lj_ir_kint(J, 1));
      J->base[1] = emitir(IRTI(IR_ADD), pos, trplen);
      rd->nres = 2;
    } else {
      emitir(IRTG(IR_EQ, IRT_PTR), trfindptr, trnullptr);
      J->base[0] = TREF_NIL;
    }
  } else {  /* Search for pattern. */
    recff_nyiu(J);
    return;
  }
}

static void recff_string_format(jit_State *J, RecordFFData *rd)
{
  TRef trfmt, tr, hdr;
  const GCstr *strfmt;
  const char *fmt, *fmt_end;
  int arg = 1;

  if (!(J->flags & JIT_F_OPT_JITSTR))
    recff_nyi(J, rd);

  trfmt = lj_ir_tostr(J, J->base[0]);
  strfmt = argv2str(J, &rd->argv[0]);
  fmt = strdata(strfmt);
  fmt_end = fmt + strfmt->len;

  /* Specialize to the format string. */
  emitir(IRTG(IR_EQ, IRT_STR), trfmt, lj_ir_kstr(J, strfmt));

  tr = hdr = recff_bufhdr(J);
  while (fmt < fmt_end) {
    TRef tra;

    if (*fmt != '%') {
      const char *start = fmt;

      while(*fmt != '%' && fmt < fmt_end)
        fmt++;
      tr = emitir(IRT(IR_BUFPUT, IRT_PTR), tr,
                  lj_ir_kstr(J, uj_str_new(J->L, start, fmt - start)));
      continue;
    }
    if (*++fmt == '%') {  /* %% */
      tr = emitir(IRT(IR_BUFPUT, IRT_PTR), tr,
                  lj_ir_kstr(J, uj_str_new(J->L, "%", 1)));
      ++fmt;
      continue;
    }

    tra = J->base[arg++];

    if (!tra)
      return; /* Interpreter will throw */

    switch (*fmt++) {
    case 'd':  case 'i': {  /* %d and %i */
      if (tref_isinteger(tra))
        tr = emitir(IRT(IR_BUFPUT, IRT_PTR), tr,
                    emitir(IRT(IR_TOSTR, IRT_STR), tra, 0));
      else
        tr = lj_ir_call(J, IRCALL_uj_sbuf_push_numint, tr, lj_ir_tonum(J, tra));
      break;
    }
    case 's': {  /* %s */
      if (!tref_isstr(tra)) {
        if (tref_isnum(tra) || tref_isinteger(tra))
          tra = emitir(IRT(IR_TOSTR, IRT_STR), tra, 0);
        else
          recff_nyiu(J);  /* NYI: __tostring and non-string types */
      }
      tr = emitir(IRT(IR_BUFPUT, IRT_PTR), tr, tra);
      break;
    }
    default:
      recff_nyiu(J);
    }
  }

  J->base[0] = emitir(IRT(IR_BUFSTR, IRT_STR), tr, hdr);
}

/* -- Table library fast functions ---------------------------------------- */

static void recff_table_getn(jit_State *J, RecordFFData *rd)
{
  if (tref_istab(J->base[0]))
    J->base[0] = lj_ir_call(J, IRCALL_lj_tab_len, J->base[0]);
  /* else: Interpreter will throw. */
  UNUSED(rd);
}

static void recff_table_concat(jit_State *J, RecordFFData *rd)
{
  TRef tab;

  if (!(J->flags & JIT_F_OPT_JITTABCAT))
    recff_nyi(J, rd);

  tab = J->base[0];
  if (tref_istab(tab)) {
    TRef trsep, trstart, trend, tr;
    TRef trsnull = lj_ir_knull(J, IRT_STR);

    if (!tref_isnil(J->base[1]))
      trsep = lj_ir_tostr(J, J->base[1]);
    else
      trsep = trsnull;

    if (J->base[1] && !tref_isnil(J->base[2]))
      trstart = lj_opt_narrow_toint(J, J->base[2]);
    else
      trstart = lj_ir_kint(J, 1);

    if (J->base[1] && J->base[2] && !tref_isnil(J->base[3]))
      trend = lj_opt_narrow_toint(J, J->base[3]);
    else
      trend = lj_ir_call(J, IRCALL_lj_tab_len, tab);

    tr = lj_ir_call(J, IRCALL_lj_tab_concat, tab, trsep, trstart, trend,
                    lj_ir_knull(J, IRT_PTR));
    emitir(IRTG(IR_NE, IRT_STR), tr, trsnull); /* error, throw in interpreter */
    J->base[0] = tr;
  }  /* else: Interpreter will throw. */
}

static void recff_table_remove(jit_State *J, RecordFFData *rd)
{
  TRef tab = J->base[0];
  rd->nres = 0;
  if (tref_istab(tab)) {
    if (tref_isnil(J->base[1])) {  /* Simple pop: t[#t] = nil */
      TRef trlen = lj_ir_call(J, IRCALL_lj_tab_len, tab);
      GCtab *t = tabV(&rd->argv[0]);
      size_t len = lj_tab_len(t);
      emitir(IRTGI(len ? IR_NE : IR_EQ), trlen, lj_ir_kint(J, 0));
      if (len) {
        RecordIndex ix;
        ix.tab = tab;
        ix.key = trlen;
        settabV(J->L, &ix.tabv, t);
        setintV(&ix.keyv, len);
        ix.idxchain = 0;
        if (results_wanted(J) != 0) {  /* Specialize load only if needed. */
          ix.val = 0;
          J->base[0] = uj_record_indexed(J, &ix);  /* Load previous value. */
          rd->nres = 1;
          /* Assumes ix.key/ix.tab is not modified for raw uj_record_indexed(). */
        }
        ix.val = TREF_NIL;
        uj_record_indexed(J, &ix);  /* Remove value. */
      }
    } else {  /* Complex case: remove in the middle. */
      recff_nyiu(J);
    }
  }  /* else: Interpreter will throw. */
}

static void recff_table_insert(jit_State *J, RecordFFData *rd)
{
  RecordIndex ix;
  ix.tab = J->base[0];
  ix.val = J->base[1];
  rd->nres = 0;
  if (tref_istab(ix.tab) && ix.val) {
    if (!J->base[2]) {  /* Simple push: t[#t+1] = v */
      TRef trlen = lj_ir_call(J, IRCALL_lj_tab_len, ix.tab);
      GCtab *t = tabV(&rd->argv[0]);
      ix.key = emitir(IRTI(IR_ADD), trlen, lj_ir_kint(J, 1));
      settabV(J->L, &ix.tabv, t);
      setintV(&ix.keyv, lj_tab_len(t) + 1);
      ix.idxchain = 0;
      uj_record_indexed(J, &ix);  /* Set new value. */
    } else {  /* Complex case: insert in the middle. */
      recff_nyiu(J);
    }
  }  /* else: Interpreter will throw. */
}

/* -- uJIT specific library fast functions -------------------------------- */

static void recff_ujit_immutable(jit_State *J, RecordFFData *rd)
{
  if (tref_istab(J->base[0]))
    lj_ir_call(J, IRCALL_uj_obj_immutable, J->base[0]);
    /* Pass on table in J->base[0] that was modified in-place */
  /*
   * Pass through already immutable arguments or
   * interpreter will throw in case of incompatible (thread) type
   */
  UNUSED(rd);
}

static void recff_ujit_table_shallowcopy(jit_State *J, RecordFFData *rd)
{
  if (tref_istab(J->base[0]))
    J->base[0] = emitir(IRTG(IR_TDUP, IRT_TAB), J->base[0], 0);
  UNUSED(rd);
}

static void recff_ujit_table_keys(jit_State *J, RecordFFData *rd)
{
  if (tref_istab(J->base[0]))
    J->base[0] = lj_ir_call(J, IRCALL_lj_tab_keys, J->base[0]);
  UNUSED(rd);
}

static void recff_ujit_table_values(jit_State *J, RecordFFData *rd)
{
  if (tref_istab(J->base[0]))
    J->base[0] = lj_ir_call(J, IRCALL_lj_tab_values, J->base[0]);
  UNUSED(rd);
}

static void recff_ujit_table_toset(jit_State *J, RecordFFData *rd)
{
  if (tref_istab(J->base[0]))
    J->base[0] = lj_ir_call(J, IRCALL_lj_tab_toset, J->base[0]);
  UNUSED(rd);
}

static void recff_ujit_table_size(jit_State *J, RecordFFData *rd)
{
  if (tref_istab(J->base[0]))
    J->base[0] = lj_ir_call(J, IRCALL_lj_tab_size, J->base[0]);
  UNUSED(rd);
}

static void recff_ujit_table_rindex(jit_State *J, RecordFFData *rd)
{
  UNUSED(rd);

  if (J->maxslot == 0 || tref_isnil(J->base[0])) {
    J->base[0] = TREF_NIL;
  } else if (tref_istab(J->base[0])) {
    size_t i;
    TRef tr;

    if (J->maxslot == 1) {
      recff_nyiu(J); /* Corner case: too few arguments. */
    }

    if (J->maxslot > ARGBUF_MAX_SIZE) {
      recff_nyiu(J); /* Corner case: too many arguments. */
    }

    tr = J->base[0];
    for (i = 1; i < J->maxslot; i++) {
      tr = emitir(IRT(IR_TVARG, IRT_NIL), tr, J->base[i]);
    }

    tr = emitir(IRT(IR_TVARGF, IRT_PTR), tr, J->maxslot);

    J->base[0] = lj_ir_call(J, IRCALL_lj_tab_rawrindex_jit, tr);
    /*
     * By convention, any of returned values set to NULL is a signal
     * for a side exit.
     */
    emitir(IRTG(IR_NE, IRT_PTR), J->base[0], lj_ir_kkptr(J, NULL));
    /*
     * TVLOAD is emitted with temporary NIL type, will be fixed
     * later - see LJ_POST_FFSPECRET implementation.
     */
    emitir(IRTG(IR_TVLOAD, IRT_NIL), J->base[0], 0);
    J->postproc = LJ_POST_FFSPECRET;
  } else {
    recff_nyiu(J);
  }
}

/* -- ujit.math library fast functions ----------------------------------- */

/* Assume true comparison. Fixup and emit pending guard later. */
static TRef recff_helper_assume_num_eq(jit_State *J, TRef a, TRef b)
{
  lua_assert(tref_isnum(a) && tref_isnum(b));
  lj_ir_set(J, IRTG(IR_EQ, IRT_NUM), a, b);
  J->postproc = LJ_POST_FIXGUARD;
  return TREF_TRUE;
}

/* Assume false comparison. Fixup and emit pending guard later. */
static TRef recff_helper_assume_num_not_eq(jit_State *J, TRef a, TRef b)
{
  lua_assert(tref_isnum(a) && tref_isnum(b));
  lj_ir_set(J, IRTG(IR_NE, IRT_NUM), a, b);
  J->postproc = LJ_POST_FIXGUARD;
  return TREF_TRUE;
}

static void recff_ujit_math_ispinf(jit_State *J, RecordFFData *rd)
{
  TRef tr = lj_ir_tonum(J, J->base[0]);
  TRef pinf = lj_ir_knum_u64(J, LJ_PINFINITY);

  J->base[0] = recff_helper_assume_num_eq(J, tr, pinf);
  UNUSED(rd);
}

static void recff_ujit_math_isninf(jit_State *J, RecordFFData *rd)
{
  TRef tr = lj_ir_tonum(J, J->base[0]);
  TRef ninf = lj_ir_knum_u64(J, LJ_MINFINITY);

  J->base[0] = recff_helper_assume_num_eq(J, tr, ninf);
  UNUSED(rd);
}

static void recff_ujit_math_isinf(jit_State *J, RecordFFData *rd)
{
  TRef tr = lj_ir_tonum(J, J->base[0]);
  TRef pinf = lj_ir_knum_u64(J, LJ_PINFINITY);

  /* abs(n) == inf => (n == +inf or n == -inf) */
  tr = emitir(IRTN(IR_ABS), tr, lj_ir_knum_abs(J));
  J->base[0] = recff_helper_assume_num_eq(J, tr, pinf);

  UNUSED(rd);
}

static void recff_ujit_math_isnan(jit_State *J, RecordFFData *rd)
{
  TRef tr = lj_ir_tonum(J, J->base[0]);

  /* NaN is not equal to itself */
  J->base[0] = recff_helper_assume_num_not_eq(J, tr, tr);
  UNUSED(rd);
}

static void recff_ujit_math_isfinite(jit_State *J, RecordFFData *rd)
{
  TRef tr = lj_ir_tonum(J, J->base[0]);
  TRef nfin_mask = lj_ir_knum_u64(J, LJ_NFIN_MASK);

  tr = emitir(IRTN(IR_FPAND), tr, nfin_mask);
  J->base[0] = recff_helper_assume_num_not_eq(J, tr, nfin_mask);

  UNUSED(rd);
}

/* -- ujit.string library fast functions ---------------------------------- */

static void recff_ujit_string_trim(jit_State *J, RecordFFData *rd)
{
  if (tref_isstr(J->base[0]))
    J->base[0] = lj_ir_call(J, IRCALL_uj_str_trim, J->base[0]);
  UNUSED(rd);
}

/* -- Record calls to fast functions -------------------------------------- */

#include "lj_recdef.h"
LJ_STATIC_ASSERT(SIZEOF_RECFF_IDMAP == FF__MAX);

/* Record entry to a fast function or C function. */
void lj_ffrecord_func(jit_State *J)
{
  const GCfunc *fn = J->fn;
  RecordFunc recff_func;
  RecordFFData rd = {
    .argv = J->L->base,
    .nres = 1 /* Default is one result. */
  };

  lua_assert(!isluafunc(fn));
  lua_assert(fn->c.ffid < SIZEOF_RECFF_IDMAP);

  recff_func = recff_idmap[fn->c.ffid];
  lua_assert(recff_func != NULL); /* Otherwise your build chain is broken. */

  J->base[J->maxslot] = 0;  /* Mark end of arguments. */
  recff_func(J, &rd); /* Call recff_* handler. */
  if (rd.nres >= 0) {
    if (J->postproc == LJ_POST_NONE) J->postproc = LJ_POST_FFRETRY;
    lj_record_ret(J, 0, rd.nres);
  }
}

#undef IR
#undef emitir

#endif
