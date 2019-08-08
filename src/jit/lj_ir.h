/*
 * SSA IR (Intermediate Representation) format.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _LJ_IR_H
#define _LJ_IR_H

#include "lj_obj.h"

/* -- IR instructions ----------------------------------------------------- */

/* IR instruction definition. Order matters, see below. ORDER IR */
#define IRDEF(_) \
  /* Guarded assertions. */ \
  /* Must be properly aligned to flip opposites (^1) and (un)ordered (^4). */ \
  _(LT,         N , ref, ref) \
  _(GE,         N , ref, ref) \
  _(LE,         N , ref, ref) \
  _(GT,         N , ref, ref) \
  \
  _(ULT,        N , ref, ref) \
  _(UGE,        N , ref, ref) \
  _(ULE,        N , ref, ref) \
  _(UGT,        N , ref, ref) \
  \
  _(EQ,         C , ref, ref) \
  _(NE,         C , ref, ref) \
  \
  _(ABC,        N , ref, ref) \
  _(RETF,       S , ref, ref) \
  \
  /* Miscellaneous ops. */ \
  _(NOP,        N , ___, ___) \
  _(BASE,       N , lit, lit) \
  _(PVAL,       N , lit, ___) \
  _(GCSTEP,     S , ___, ___) \
  _(HIOP,       S , ref, ref) \
  _(LOOP,       S , ___, ___) \
  _(USE,        S , ref, ___) \
  _(PHI,        S , ref, ref) \
  _(RENAME,     S , ref, lit) \
  _(TVARG,      S , ref, ref) \
  _(TVARGF,     S , ref, lit) \
  \
  /* Constants. */ \
  _(KPRI,       N , ___, ___) \
  _(KINT,       N , cst, ___) \
  _(KGC,        N , cst, ___) \
  _(KPTR,       N , cst, ___) \
  _(KKPTR,      N , cst, ___) \
  _(KNULL,      N , cst, ___) \
  _(KNUM,       N , cst, ___) \
  _(KINT64,     N , cst, ___) \
  _(KSLOT,      N , ref, lit) \
  \
  /* Bit ops. */ \
  _(BNOT,       N , ref, ___) \
  _(BSWAP,      N , ref, ___) \
  _(BAND,       C , ref, ref) \
  _(BOR,        C , ref, ref) \
  _(BXOR,       C , ref, ref) \
  _(BSHL,       N , ref, ref) \
  _(BSHR,       N , ref, ref) \
  _(BSAR,       N , ref, ref) \
  _(BROL,       N , ref, ref) \
  _(BROR,       N , ref, ref) \
  \
  /* Arithmetic ops. ORDER ARITH */ \
  _(ADD,        C , ref, ref) \
  _(SUB,        N , ref, ref) \
  _(MUL,        C , ref, ref) \
  _(DIV,        N , ref, ref) \
  _(MOD,        N , ref, ref) \
  _(POW,        N , ref, ref) \
  _(NEG,        N , ref, ref) \
  \
  _(ABS,        N , ref, ref) \
  _(ATAN2,      N , ref, ref) \
  _(LDEXP,      N , ref, ref) \
  _(MIN,        C , ref, ref) \
  _(MAX,        C , ref, ref) \
  _(FPMATH,     N , ref, lit) \
  _(FPAND,      N , ref, ref) \
  \
  /* Overflow-checking arithmetic ops. */ \
  _(ADDOV,      CW, ref, ref) \
  _(SUBOV,      NW, ref, ref) \
  _(MULOV,      CW, ref, ref) \
  \
  /* Memory ops. A = array, H = hash, U = upvalue, F = field, S = stack. */ \
  \
  /* Memory references. */ \
  _(AREF,       R , ref, ref) \
  _(HREFK,      R , ref, ref) \
  _(HREF,       L , ref, ref) \
  _(NEWREF,     S , ref, ref) \
  _(UREFO,      LW, ref, lit) \
  _(UREFC,      LW, ref, lit) \
  _(FREF,       R , ref, lit) \
  _(STRREF,     N , ref, ref) \
  _(LREF  ,     L , ___, ___) \
  \
  /* Loads and Stores. These must be in the same order. */ \
  _(HKLOAD,     L , ref, ___) \
  _(ALOAD,      L , ref, ___) \
  _(HLOAD,      L , ref, ___) \
  _(ULOAD,      L , ref, ___) \
  _(FLOAD,      L , ref, lit) \
  _(XLOAD,      L , ref, lit) \
  _(SLOAD,      L , lit, lit) \
  _(VLOAD,      L , ref, ___) \
  _(TVLOAD,     L , ref, ___) \
  \
  _(ASTORE,     S , ref, ref) \
  _(HSTORE,     S , ref, ref) \
  _(USTORE,     S , ref, ref) \
  _(FSTORE,     S , ref, ref) \
  _(XSTORE,     S , ref, ref) \
  \
  /* Allocations. */ \
  _(SNEW,       N , ref, ref)  /* CSE is ok, not marked as A. */ \
  _(XSNEW,      A , ref, ref) \
  _(TNEW,       AW, lit, lit) \
  _(TDUP,       AW, ref, ___) \
  _(CNEW,       AW, ref, ref) \
  _(CNEWI,      NW, ref, ref)  /* CSE is ok, not marked as A. */ \
  \
  /* Buffer operations. */ \
  _(BUFHDR,     L , ref, lit) \
  _(BUFPUT,     L , ref, ref) \
  _(BUFSTR,     A , ref, ref) \
  \
  /* Barriers. */ \
  _(TBAR,       S , ref, ___) \
  _(OBAR,       S , ref, ref) \
  _(XBAR,       S , ___, ___) \
  \
  /* Type conversions. */ \
  _(CONV,       NW, ref, lit) \
  _(TOBIT,      N , ref, ref) \
  _(TOSTR,      N , ref, ___) \
  _(STRTO,      N , ref, ___) \
  \
  /* Calls. */ \
  _(CALLN,      N , ref, lit) \
  _(CALLL,      L , ref, lit) \
  _(CALLS,      S , ref, lit) \
  _(CALLXS,     S , ref, ref) \
  _(CARG,       N , ref, ref) \
  \
  /* End of list. */

/* IR opcodes (max. 256). */
typedef enum {
#define IRENUM(name, m, m1, m2) IR_##name,
IRDEF(IRENUM)
#undef IRENUM
  IR__MAX
} IROp;

/* Stored opcode. */
typedef uint8_t IROp1;

LJ_STATIC_ASSERT(((int)IR_EQ^1) == (int)IR_NE);
LJ_STATIC_ASSERT(((int)IR_LT^1) == (int)IR_GE);
LJ_STATIC_ASSERT(((int)IR_LE^1) == (int)IR_GT);
LJ_STATIC_ASSERT(((int)IR_LT^3) == (int)IR_GT);
LJ_STATIC_ASSERT(((int)IR_LT^4) == (int)IR_ULT);

/* Delta between xLOAD and xSTORE. */
#define IRDELTA_L2S             ((int)IR_ASTORE - (int)IR_ALOAD)

LJ_STATIC_ASSERT((int)IR_HLOAD + IRDELTA_L2S == (int)IR_HSTORE);
LJ_STATIC_ASSERT((int)IR_ULOAD + IRDELTA_L2S == (int)IR_USTORE);
LJ_STATIC_ASSERT((int)IR_FLOAD + IRDELTA_L2S == (int)IR_FSTORE);
LJ_STATIC_ASSERT((int)IR_XLOAD + IRDELTA_L2S == (int)IR_XSTORE);

/* -- Named IR literals --------------------------------------------------- */

/* FPMATH sub-functions. ORDER FPM. */
#define IRFPMDEF(_) \
  _(FLOOR) _(CEIL) _(TRUNC)  /* Must be first and in this order. */ \
  _(SQRT) _(EXP) _(EXP2) _(LOG) _(LOG2) _(LOG10) \
  _(SIN) _(COS) _(TAN) \
  _(OTHER)

typedef enum {
#define FPMENUM(name)           IRFPM_##name,
IRFPMDEF(FPMENUM)
#undef FPMENUM
  IRFPM__MAX
} IRFPMathOp;

/* FLOAD fields. */
#define IRFLDEF(_) \
  _(GCO_MARKED, offsetof(GCobj, gch.marked)) \
  _(STR_LEN,    offsetof(GCstr, len)) \
  _(FUNC_ENV,   offsetof(GCfunc, l.env)) \
  _(FUNC_PC,    offsetof(GCfunc, l.pc)) \
  _(FUNC_FFID,  offsetof(GCfunc, c.ffid)) \
  _(THREAD_ENV, offsetof(lua_State, env)) \
  _(TAB_META,   offsetof(GCtab, metatable)) \
  _(TAB_ARRAY,  offsetof(GCtab, array)) \
  _(TAB_NODE,   offsetof(GCtab, node)) \
  _(TAB_ASIZE,  offsetof(GCtab, asize)) \
  _(TAB_HMASK,  offsetof(GCtab, hmask)) \
  _(TAB_NOMM,   offsetof(GCtab, nomm)) \
  _(UDATA_META, offsetof(GCudata, metatable)) \
  _(UDATA_UDTYPE, offsetof(GCudata, udtype)) \
  _(UDATA_FILE, sizeof(GCudata)) \
  _(CDATA_CTYPEID, offsetof(GCcdata, ctypeid)) \
  _(CDATA_PTR,  sizeof(GCcdata)) \
  _(CDATA_INT, sizeof(GCcdata)) \
  _(CDATA_INT64, sizeof(GCcdata)) \
  _(CDATA_INT64_4, sizeof(GCcdata) + 4)

typedef enum {
#define FLENUM(name, ofs)       IRFL_##name,
IRFLDEF(FLENUM)
#undef FLENUM
  IRFL__MAX
} IRFieldID;

/* SLOAD mode bits, stored in op2. */
#define IRSLOAD_PARENT          0x01    /* Coalesce with parent trace. */
#define IRSLOAD_FRAME           0x02    /* Load hiword of frame. */
#define IRSLOAD_TYPECHECK       0x04    /* Needs type check. */
#define IRSLOAD_CONVERT         0x08    /* Number to integer conversion. */
#define IRSLOAD_READONLY        0x10    /* Read-only, omit slot store. */
#define IRSLOAD_INHERIT         0x20    /* Inherited by exits/side traces. */

#define ir_sload_typecheck(ir) \
  ((ir)->op2 & IRSLOAD_TYPECHECK)

/* Calculate SLOAD offset from BASE depending on the desired output (TValue payload of regular
** stack slot or frame information of the frame link).
*/
#define ir_sload_offset(ir) \
  ((int32_t)sizeof(TValue)*((int32_t)(ir)->op1-1) + (((ir)->op2&IRSLOAD_FRAME)?offsetof(TValue, fr.tp):0))

/* XLOAD mode, stored in op2. */
#define IRXLOAD_READONLY        1       /* Load from read-only data. */
#define IRXLOAD_VOLATILE        2       /* Load from volatile data. */
#define IRXLOAD_UNALIGNED       4       /* Unaligned load. */

/* BUFHDR mode, stored in op2 */
#define IRBUFHDR_RESET          0       /* Reset buffer. */
#define IRBUFHDR_APPEND         1       /* Append to buffer. */

/* CONV mode, stored in op2. */
#define IRCONV_SRCMASK          0x001f  /* Source IRType. */
#define IRCONV_DSTMASK          0x03e0  /* Dest. IRType (also in ir->t). */
#define IRCONV_DSH              5
#define IRCONV_NUM_INT          ((IRT_NUM<<IRCONV_DSH)|IRT_INT)
#define IRCONV_INT_NUM          ((IRT_INT<<IRCONV_DSH)|IRT_NUM)
#define IRCONV_TRUNC            0x0400  /* Truncate number to integer. */
#define IRCONV_SEXT             0x0800  /* Sign-extend integer to integer. */
#define IRCONV_MODEMASK         0x0fff
#define IRCONV_CONVMASK         0xf000
#define IRCONV_CSH              12
/* Number to integer conversion mode. Ordered by strength of the checks. */
#define IRCONV_TOBIT  (0<<IRCONV_CSH)   /* None. Cache only: TOBIT conv. */
#define IRCONV_ANY    (1<<IRCONV_CSH)   /* Any FP number is ok. */
#define IRCONV_INDEX  (2<<IRCONV_CSH)   /* Check + special backprop rules. */
#define IRCONV_CHECK  (3<<IRCONV_CSH)   /* Number checked for integerness. */

/* -- IR operands --------------------------------------------------------- */

/* IR operand mode (2 bit). */
typedef enum {
  IRMref,               /* IR reference. */
  IRMlit,               /* 16 bit unsigned literal. */
  IRMcst,               /* Constant literal: i, gcr or ptr. */
  IRMnone               /* Unused operand. */
} IRMode;
#define IRM___          IRMnone

/* Mode bits: Commutative, {Normal/Ref, Alloc, Load, Store}, Non-weak guard. */
#define IRM_C                   0x10

#define IRM_N                   0x00
#define IRM_R                   IRM_N
#define IRM_A                   0x20
#define IRM_L                   0x40
#define IRM_S                   0x60

#define IRM_W                   0x80

#define IRM_NW                  (IRM_N|IRM_W)
#define IRM_CW                  (IRM_C|IRM_W)
#define IRM_AW                  (IRM_A|IRM_W)
#define IRM_LW                  (IRM_L|IRM_W)

#define irm_op1(m)              ((IRMode)((m)&3))
#define irm_op2(m)              ((IRMode)(((m)>>2)&3))
#define irm_iscomm(m)           ((m) & IRM_C)
#define irm_kind(m)             ((m) & IRM_S)

#define IRMODE(name, m, m1, m2) (((IRM##m1)|((IRM##m2)<<2)|(IRM_##m))^IRM_W),

LJ_DATA const uint8_t lj_ir_mode[IR__MAX+1];

/* -- IR instruction types ------------------------------------------------ */

/* Map of itypes to non-negative numbers. ORDER LJ_T.
** LJ_TUPVAL/LJ_TTRACE never appear in a TValue. Use these itypes for
** IRT_P32 and IRT_P64, which never escape the IR.
** The various integers are only used in the IR and can only escape to
** a TValue after implicit or explicit conversion. Their types must be
** contiguous and next to IRT_NUM (see the typerange macros below).
*/
#define IRTDEF(_) \
  _(NIL, 8) _(FALSE, 4) _(TRUE, 4) _(LIGHTUD, 8) _(STR, 8) \
  _(P32, 8) _(THREAD, 8) _(PROTO, 8) _(FUNC, 8) _(P64, 8) _(CDATA, 8) \
  _(TAB, 8) _(UDATA, 8) \
  _(NUM, 8) _(FLOAT, 4) _(I8, 1) _(U8, 1) _(I16, 2) _(U16, 2) \
  _(INT, 4) _(U32, 4) _(I64, 8) _(U64, 8) \
  _(TVAL, 16)
  /* There is room for 8 more types. */

/* IR result type and flags (8 bit). */
typedef enum {
#define IRTENUM(name, size)     IRT_##name,
IRTDEF(IRTENUM)
#undef IRTENUM
  IRT__MAX,

  /* Native pointer type and the corresponding integer type. */
  IRT_PTR   = IRT_P64,
  IRT_INTP  = IRT_I64,
  IRT_UINTP = IRT_U64,

  /* Additional flags. */
  IRT_MARK = 0x20,      /* Marker for misc. purposes. */
  IRT_ISPHI = 0x40,     /* Instruction is left or right PHI operand. */
  IRT_GUARD = 0x80,     /* Instruction is a guard. */

  /* Masks. */
  IRT_TYPE = 0x1f,
  IRT_T = 0xff
} IRType;

#define irtype_ispri(irt)       ((uint32_t)(irt) <= IRT_TRUE)

/* Stored IRType. */
typedef struct IRType1 { uint8_t irt; } IRType1;

#define IRT(o, t)               ((uint32_t)(((o)<<8) | (t)))
#define IRTI(o)                 (IRT((o), IRT_INT))
#define IRTN(o)                 (IRT((o), IRT_NUM))
#define IRTG(o, t)              (IRT((o), IRT_GUARD|(t)))
#define IRTGI(o)                (IRT((o), IRT_GUARD|IRT_INT))

#define irt_t(t)                ((IRType)(t).irt)
#define irt_type(t)             ((IRType)((t).irt & IRT_TYPE))
#define irt_sametype(t1, t2)    ((((t1).irt ^ (t2).irt) & IRT_TYPE) == 0)
#define irt_typerange(t, first, last) \
  ((uint32_t)((t).irt & IRT_TYPE) - (uint32_t)(first) <= \
  (uint32_t)((last) - (first)))

#define irt_isnil(t)            (irt_type(t) == IRT_NIL)
#define irt_ispri(t)            ((uint32_t)irt_type(t) <= IRT_TRUE)
#define irt_islightud(t)        (irt_type(t) == IRT_LIGHTUD)
#define irt_isstr(t)            (irt_type(t) == IRT_STR)
#define irt_istab(t)            (irt_type(t) == IRT_TAB)
#define irt_iscdata(t)          (irt_type(t) == IRT_CDATA)
#define irt_isfloat(t)          (irt_type(t) == IRT_FLOAT)
#define irt_isnum(t)            (irt_type(t) == IRT_NUM)
#define irt_isint(t)            (irt_type(t) == IRT_INT)
#define irt_isi8(t)             (irt_type(t) == IRT_I8)
#define irt_isu8(t)             (irt_type(t) == IRT_U8)
#define irt_isi16(t)            (irt_type(t) == IRT_I16)
#define irt_isu16(t)            (irt_type(t) == IRT_U16)
#define irt_isu32(t)            (irt_type(t) == IRT_U32)
#define irt_isi64(t)            (irt_type(t) == IRT_I64)
#define irt_isu64(t)            (irt_type(t) == IRT_U64)
#define irt_istval(t)           (irt_type(t) == IRT_TVAL)

#define irt_isfp(t)             (irt_isnum(t) || irt_isfloat(t))
#define irt_isinteger(t)        (irt_typerange((t), IRT_I8, IRT_INT))
#define irt_isgcv(t)            (irt_typerange((t), IRT_STR, IRT_UDATA))
#define irt_isaddr(t)           (irt_typerange((t), IRT_LIGHTUD, IRT_UDATA))
#define irt_isint64(t)          (irt_typerange((t), IRT_I64, IRT_U64))

#define irt_is64(t)             (8 == irt_size((t)))
#define irt_is64orfp(t)         (irt_is64(t) || irt_isfloat(t))

#define irt_size(t)             (lj_ir_type_size[irt_type((t))])

#define irt_kgc(ir)             ((ir)->o == IR_KGC)

LJ_DATA const uint8_t lj_ir_type_size[];

static LJ_AINLINE IRType itype2irt(const TValue *tv) {
  lua_assert(gettag(tv) >= LJ_TNUMX);
  return (IRType)~gettag(tv);
}

static LJ_AINLINE uint32_t irt_toitype_(IRType t) {
  lua_assert(t <= IRT_NUM);
  return ~(uint32_t)t;
}

#define irt_toitype(t)          irt_toitype_(irt_type((t)))

#define irt_isguard(t)          ((t).irt & IRT_GUARD)
#define irt_ismarked(t)         ((t).irt & IRT_MARK)
#define irt_setmark(t)          ((t).irt |= IRT_MARK)
#define irt_clearmark(t)        ((t).irt &= ~IRT_MARK)
#define irt_isphi(t)            ((t).irt & IRT_ISPHI)
#define irt_setphi(t)           ((t).irt |= IRT_ISPHI)
#define irt_clearphi(t)         ((t).irt &= ~IRT_ISPHI)

/* Stored combined IR opcode and type. */
typedef uint16_t IROpT;

/* -- IR references ------------------------------------------------------- */

/* IR references. */
typedef uint16_t IRRef1;        /* One stored reference. */
typedef uint32_t IRRef2;        /* Two stored references. */
typedef uint32_t IRRef;         /* Used to pass around references. */

/* Fixed references. */
enum {
  REF_BIAS =    0x8000,
  REF_TRUE =    REF_BIAS-3,
  REF_FALSE =   REF_BIAS-2,
  REF_NIL =     REF_BIAS-1,     /* \--- Constants grow downwards. */
  REF_BASE =    REF_BIAS,       /* /--- IR grows upwards. */
  REF_FIRST =   REF_BIAS+1,
  REF_DROP =    0xffff
};

/* Note: IRMlit operands must be < REF_BIAS, too!
** This allows for fast and uniform manipulation of all operands
** without looking up the operand mode in lj_ir_mode:
** - CSE calculates the maximum reference of two operands.
**   This must work with mixed reference/literal operands, too.
** - DCE marking only checks for operand >= REF_BIAS.
** - LOOP needs to substitute reference operands.
**   Constant references and literals must not be modified.
*/

#define IRREF2(lo, hi)          ((IRRef2)(lo) | ((IRRef2)(hi) << 16))

#define irref_isk(ref)          ((ref) < REF_BIAS)

/* Tagged IR references (32 bit).
**
** +-------+-------+---------------+
** |  irt  | flags |      ref      |
** +-------+-------+---------------+
**
** The tag holds a copy of the IRType and speeds up IR type checks.
*/
typedef uint32_t TRef;

#define TREF_REFMASK            0x0000ffff
#define TREF_FRAME              0x00010000
#define TREF_CONT               0x00020000
#define TREF_LOW                0x00040000
#define TREF_FLAGMASK           (TREF_FRAME | TREF_CONT | TREF_LOW)

#define TREF(ref, t)            ((TRef)((ref) + ((t)<<24)))

#define tref_ref(tr)            ((IRRef1)(tr))
#define tref_t(tr)              ((IRType)((tr)>>24))
#define tref_type(tr)           ((IRType)(((tr)>>24) & IRT_TYPE))
#define tref_typerange(tr, first, last) \
  ((((tr)>>24) & IRT_TYPE) - (TRef)(first) <= (TRef)((last) - (first)))

#define tref_istype(tr, t)      (((tr) & (IRT_TYPE<<24)) == ((t)<<24))
#define tref_isnil(tr)          (tref_istype((tr), IRT_NIL))
#define tref_isfalse(tr)        (tref_istype((tr), IRT_FALSE))
#define tref_istrue(tr)         (tref_istype((tr), IRT_TRUE))
#define tref_isstr(tr)          (tref_istype((tr), IRT_STR))
#define tref_isfunc(tr)         (tref_istype((tr), IRT_FUNC))
#define tref_iscdata(tr)        (tref_istype((tr), IRT_CDATA))
#define tref_istab(tr)          (tref_istype((tr), IRT_TAB))
#define tref_isudata(tr)        (tref_istype((tr), IRT_UDATA))
#define tref_isnum(tr)          (tref_istype((tr), IRT_NUM))
#define tref_isint(tr)          (tref_istype((tr), IRT_INT))

#define tref_isbool(tr)         (tref_typerange((tr), IRT_FALSE, IRT_TRUE))
#define tref_ispri(tr)          (tref_typerange((tr), IRT_NIL, IRT_TRUE))
#define tref_istruecond(tr)     (!tref_typerange((tr), IRT_NIL, IRT_FALSE))
#define tref_isinteger(tr)      (tref_typerange((tr), IRT_I8, IRT_INT))
#define tref_isnumber(tr)       (tref_typerange((tr), IRT_NUM, IRT_U64))
#define tref_isnumber_str(tr)   (tref_isnumber((tr)) || tref_isstr((tr)))
#define tref_isgcv(tr)          (tref_typerange((tr), IRT_STR, IRT_UDATA))

#define tref_isk(tr)            (irref_isk(tref_ref((tr))))
#define tref_isk2(tr1, tr2)     (irref_isk(tref_ref((tr1) | (tr2))))

#define TREF_PRI(t)             (TREF(REF_NIL-(t), (t)))
#define TREF_NIL                (TREF_PRI(IRT_NIL))
#define TREF_FALSE              (TREF_PRI(IRT_FALSE))
#define TREF_TRUE               (TREF_PRI(IRT_TRUE))

/* -- IR format ----------------------------------------------------------- */

/* IR instruction format (64 bit).
**
**    16      16          32         8   8   8   8
** +-------+-------+---------------+---+---+---+---+
** |  op1  |  op2  |    padding    | t | o | r | s |
** +-------+-------+---------------+---+---+---+---+
** |  op12/i/gco   | i/gco hidword |   ot  | prev  | (alternative fields in union)
** +---------------+---------------+-------+-------+
**        32              32           16     16
**
** prev is only valid prior to register allocation and then reused for r + s.
*/

typedef struct IRIns {
  union {
    int64_t i;
    void *ptr;
    struct {
      union {
        IRRef2 op12;     /* IR operand 1 and 2. */
        struct {
          IRRef1 op1;    /* IR operand 1. */
          IRRef1 op2;    /* IR operand 2. */
        };
      };
      uint8_t hints;     /* IR hints */
      uint8_t padding1;
      uint16_t padding2;
    };
  };
  union {
    IROpT ot;            /* IR opcode and type. */
    struct {
      IRType1 t;         /* IR type. */
      IROp1 o;           /* IR opcode. */
    };
  };
  union {
    IRRef1 prev;         /* Previous ins in same chain (overlaps r and s). */
    struct {
      uint8_t r;         /* Register allocation (overlaps prev). */
      uint8_t s;         /* Spill slot allocation (overlaps prev). */
    };
  };
} IRIns;

/* IR hints: */

#define IRH_TAB_METAIDX     0x01 /* Table has a metatable during indexing */
#define IRH_TAB_SETMETA     0x02 /* Table has a setmetatable call on trace
                                    (with a non-nil 2nd argument) */
#define IRH_TAB_NOMETAGUARD 0x04 /* Table has a mt == NULL guard on trace;
                                    the guard may or may not be eliminated by
                                    other optimizations (e.g. FWD eliminates
                                    guards for tables returned by IR_TDUP). */
#define IRH_MOVTV           0x08 /* IR takes part in MOVTV optimisation */
#define IRH_LOAD_KEEPGUARD  0x10 /* Do not drop a guard from a load
                                    under any circumstances. At the moment this
                                    hint is used by the MOVTV optimization. */
#define IRH_MARK            0x80 /* Multi-purpose mark */

static LJ_AINLINE int ir_hashint(const IRIns *ir, uint8_t h)
{
  return (ir->hints & h);
}

static LJ_AINLINE void ir_sethint(IRIns *ir, uint8_t h)
{
  ir->hints |= h;
}

static LJ_AINLINE void ir_clearhint(IRIns *ir, uint8_t h)
{
  ir->hints &= (uint8_t)(~h);
}

#define ir_kgc(ir)      lua_check((ir)->o == IR_KGC, ((GCobj*)((ir)->ptr)))
#define ir_kstr(ir)     (gco2str(ir_kgc((ir))))
#define ir_ktab(ir)     (gco2tab(ir_kgc((ir))))
#define ir_kfunc(ir)    (gco2func(ir_kgc((ir))))
#define ir_kproto(ir)   (gco2pt(ir_kgc((ir))))
#define ir_kcdata(ir)   (gco2cd(ir_kgc((ir))))
#define ir_knum(ir)     lua_check((ir)->o == IR_KNUM, ((const TValue*)((ir)->ptr)))
#define ir_kint64(ir)   lua_check((ir)->o == IR_KINT64, ((const TValue*)((ir)->ptr)))
#define ir_k64(ir) \
  lua_check((ir)->o == IR_KNUM || (ir)->o == IR_KINT64, ((const TValue*)((ir)->ptr)))
#define ir_kptr(ir) \
  lua_check((ir)->o == IR_KPTR || (ir)->o == IR_KKPTR, ((ir)->ptr))

/* A store or any other op with a non-weak guard has a side-effect. */
static LJ_AINLINE int ir_sideeff(IRIns *ir)
{
  return (((ir->t.irt | ~IRT_GUARD) & lj_ir_mode[ir->o]) >= IRM_S);
}

/* Replaces IR instruction with nop. Assumes that surrounding instructions are already re-linked.
*/
static LJ_AINLINE void ir_tonop(IRIns *ir) {
  ir->o = IR_NOP;
  ir->t.irt = IRT_NIL;
  ir->op1 = ir->op2 = 0;
  ir->prev = 0;
}

/* Maximum slot index that can be stored as a second operand of KSLOT instruction.
** Should obviously be maximum unsigned value held by IRRef1, see below.
*/
#define MAX_KSLOT (0xffffu)
LJ_STATIC_ASSERT(MAX_KSLOT == (1u << (sizeof(IRRef1) * CHAR_BIT)) - 1u);

LJ_STATIC_ASSERT((int)IRT_GUARD == (int)IRM_W);

#endif
