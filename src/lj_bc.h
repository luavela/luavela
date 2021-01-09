/*
 * Bytecode instruction format.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _LJ_BC_H
#define _LJ_BC_H

#include "lj_def.h"
#include "uj_arch.h"
#include "lj_bcins.h"
#include "lj_vm.h"

/* Bytecode instruction format, 32 bit wide, fields of 8 or 16 bit:
**
** +----+----+----+----+
** | B  | C  | A  | OP | Format ABC
** +----+----+----+----+
** |    D    | A  | OP | Format AD
** +--------------------
** MSB               LSB
**
** Layout for format AJ (jump instructions) is the same as for format AD
** except that jump target is biased with BCBIAS_J.
**
** If an operand holds a stack slot offset, offset's doubled value is stored
** in the instruction, e.g.:
**
** KSHORT 5 32767 ==> is encoded as ==> 0xffff0a1d
**
** Rationale: During migration to x64, stack slot size (sizeof(TValue)) grew to
** 16 bytes which effectively disabled SIB addressing in the VM. Storing slot
** offsets *x2-encoded* allows to re-enable SIB addressing.
**
** In-memory instructions are always stored in host byte order.
*/

/* Operand ranges and related constants. */
#define BCMAX_A         0xff
#define BCMAX_B         0xff
#define BCMAX_C         0xff
#define BCMAX_D         0xffff
#define BCBIAS_J        0x8000
#define NO_REG          BCMAX_A
#define NO_JMP          (~(BCPos)0)

LJ_DATA const uint16_t lj_bc_mode[];
LJ_DATA const ASMFunction lj_bc_ptr[];

/* Bytecode instruction definition. Order matters, see below.
**
** (name, filler, Amode, Bmode, Cmode or Dmode, metamethod)
**
** The opcode name suffixes specify the type for RB/RC or RD:
** V = variable slot
** S = string const
** N = number const
** P = primitive type (~itype)
** B = unsigned byte literal
** M = multiple args/results
*/
#define BCDEF(_) \
  /* Comparison ops. ORDER OPR. */ \
  /* 0x00 */ _(ISLT,    var,    ___,    var,    lt) \
  /* 0x01 */ _(ISGE,    var,    ___,    var,    lt) \
  /* 0x02 */ _(ISLE,    var,    ___,    var,    le) \
  /* 0x03 */ _(ISGT,    var,    ___,    var,    le) \
  \
  /* 0x04 */ _(ISEQV,   var,    ___,    var,    eq) \
  /* 0x05 */ _(ISNEV,   var,    ___,    var,    eq) \
  /* 0x06 */ _(ISEQS,   var,    ___,    str,    eq) \
  /* 0x07 */ _(ISNES,   var,    ___,    str,    eq) \
  /* 0x08 */ _(ISEQN,   var,    ___,    num,    eq) \
  /* 0x09 */ _(ISNEN,   var,    ___,    num,    eq) \
  /* 0x0a */ _(ISEQP,   var,    ___,    pri,    eq) \
  /* 0x0b */ _(ISNEP,   var,    ___,    pri,    eq) \
  \
  /* Unary test and copy ops. */ \
  /* 0x0c */ _(ISTC,    dst,    ___,    var,    ___) \
  /* 0x0d */ _(ISFC,    dst,    ___,    var,    ___) \
  /* 0x0e */ _(IST,     ___,    ___,    var,    ___) \
  /* 0x0f */ _(ISF,     ___,    ___,    var,    ___) \
  \
  /* Unary ops. */ \
  /* 0x10 */ _(MOV,     dst,    ___,    var,    ___) \
  /* 0x11 */ _(NOT,     dst,    ___,    var,    ___) \
  /* 0x12 */ _(UNM,     dst,    ___,    var,    unm) \
  /* 0x13 */ _(LEN,     dst,    ___,    var,    len) \
  \
  /* Binary ops. ORDER OPR. VV last, POW must be next. */ \
  /* 0x14 */ _(ADD,     dst,    var,    var,    add) \
  /* 0x15 */ _(SUB,     dst,    var,    var,    sub) \
  /* 0x16 */ _(MUL,     dst,    var,    var,    mul) \
  /* 0x17 */ _(DIV,     dst,    var,    var,    div) \
  /* 0x18 */ _(MOD,     dst,    var,    var,    mod) \
  \
  /* 0x19 */ _(POW,     dst,    var,    var,    pow) \
  /* 0x1a */ _(CAT,     dst,    base,   base,   concat) \
  \
  /* Constant ops. */ \
  /* 0x1b */ _(KSTR,    dst,    ___,    str,    ___) \
  /* 0x1c */ _(KCDATA,  dst,    ___,    cdata,  ___) \
  /* 0x1d */ _(KSHORT,  dst,    ___,    lits,   ___) \
  /* 0x1e */ _(KNUM,    dst,    ___,    num,    ___) \
  /* 0x1f */ _(KPRI,    dst,    ___,    pri,    ___) \
  /* 0x20 */ _(KNIL,    base,   ___,    base,   ___) \
  \
  /* Upvalue and function ops. */ \
  /* 0x21 */ _(UGET,    dst,    ___,    uv,     ___) \
  /* 0x22 */ _(USETV,   uv,     ___,    var,    ___) \
  /* 0x23 */ _(USETS,   uv,     ___,    str,    ___) \
  /* 0x24 */ _(USETN,   uv,     ___,    num,    ___) \
  /* 0x25 */ _(USETP,   uv,     ___,    pri,    ___) \
  /* 0x26 */ _(UCLO,    rbase,  ___,    jump,   ___) \
  /* 0x27 */ _(FNEW,    dst,    ___,    func,   gc) \
  \
  /* Table ops. */ \
  /* 0x28 */ _(TNEW,    dst,    ___,    lit,    gc) \
  /* 0x29 */ _(TDUP,    dst,    ___,    tab,    gc) \
  /* 0x2a */ _(GGET,    dst,    ___,    str,    index) \
  /* 0x2b */ _(GSET,    var,    ___,    str,    newindex) \
  /* 0x2c */ _(TGETV,   dst,    var,    var,    index) \
  /* 0x2d */ _(TGETS,   dst,    var,    str,    index) \
  /* 0x2e */ _(TGETB,   dst,    var,    lit,    index) \
  /* 0x2f */ _(TSETV,   var,    var,    var,    newindex) \
  /* 0x30 */ _(TSETS,   var,    var,    str,    newindex) \
  /* 0x31 */ _(TSETB,   var,    var,    lit,    newindex) \
  /* 0x32 */ _(TSETM,   base,   ___,    num,    newindex) \
  \
  /* Calls and vararg handling. T = tail call. */ \
  /* 0x33 */ _(CALLM,   base,   lit,    lit,    call) \
  /* 0x34 */ _(CALL,    base,   lit,    lit,    call) \
  /* 0x35 */ _(CALLMT,  base,   ___,    lit,    call) \
  /* 0x36 */ _(CALLT,   base,   ___,    lit,    call) \
  /* 0x37 */ _(ITERC,   base,   lit,    lit,    call) \
  /* 0x38 */ _(ITERN,   base,   lit,    lit,    call) \
  /* 0x39 */ _(VARG,    base,   lit,    lit,    ___) \
  /* 0x3a */ _(ISNEXT,  base,   ___,    jump,   ___) \
  \
  /* Returns. */ \
  /* 0x3b */ _(RETM,    base,   ___,    lit,    ___) \
  /* 0x3c */ _(RET,     rbase,  ___,    lit,    ___) \
  /* 0x3d */ _(RET0,    rbase,  ___,    lit,    ___) \
  /* 0x3e */ _(RET1,    rbase,  ___,    lit,    ___) \
  \
  /* Hotcounting */ \
  /* 0x3f */ _(HOTCNT,  ___,    ___,    ___,    ___) \
  \
  /* Coverage counting */ \
  /* 0x40 */ _(COVERG,  ___,    ___,    ___,    ___) \
  \
  /* Loops and branches. I/J = interp/JIT, I/C/L = init/call/loop. */ \
  /* 0x41 */ _(FORI,    base,   ___,    jump,   ___) \
  /* 0x42 */ _(JFORI,   base,   ___,    jump,   ___) \
  \
  /* 0x43 */ _(FORL,    base,   ___,    jump,   ___) \
  /* 0x44 */ _(IFORL,   base,   ___,    jump,   ___) \
  /* 0x45 */ _(JFORL,   base,   ___,    lit,    ___) \
  \
  /* 0x46 */ _(ITERL,   base,   ___,    jump,   ___) \
  /* 0x47 */ _(IITERL,  base,   ___,    jump,   ___) \
  /* 0x48 */ _(JITERL,  base,   ___,    lit,    ___) \
  \
  /* 0x49 */ _(ITRNL,   base,   ___,    jump,   ___) \
  /* 0x4a */ _(IITRNL,  base,   ___,    jump,   ___) \
  /* 0x4b */ _(JITRNL,  base,   ___,    lit,    ___) \
  \
  /* 0x4c */ _(LOOP,    rbase,  ___,    jump,   ___) \
  /* 0x4d */ _(ILOOP,   rbase,  ___,    jump,   ___) \
  /* 0x4e */ _(JLOOP,   rbase,  ___,    lit,    ___) \
  \
  /* 0x4f */ _(JMP,     rbase,  ___,    jump,   ___) \
  \
  /* Function headers. I/J = interp/JIT, F/V/C = fixarg/vararg/C func. */ \
  /* 0x50 */ _(FUNCF,   rbase,  ___,    ___,    ___) \
  /* 0x51 */ _(IFUNCF,  rbase,  ___,    ___,    ___) \
  /* 0x52 */ _(JFUNCF,  rbase,  ___,    lit,    ___) \
  /* 0x53 */ _(FUNCV,   rbase,  ___,    ___,    ___) \
  /* 0x54 */ _(IFUNCV,  rbase,  ___,    ___,    ___) \
  /* 0x55 */ _(JFUNCV,  rbase,  ___,    lit,    ___) \
  /* 0x56 */ _(FUNCC,   rbase,  ___,    ___,    ___) \
  /* 0x57 */ _(FUNCCW,  rbase,  ___,    ___,    ___)

/* Bytecode opcode numbers. */
typedef enum {
#define BCENUM(name, ma, mb, mc, mt)    BC_##name,
BCDEF(BCENUM)
#undef BCENUM
  BC__MAX
} BCOp;

LJ_STATIC_ASSERT((int)BC_ISEQV+1 == (int)BC_ISNEV);
LJ_STATIC_ASSERT(((int)BC_ISEQV^1) == (int)BC_ISNEV);
LJ_STATIC_ASSERT(((int)BC_ISEQS^1) == (int)BC_ISNES);
LJ_STATIC_ASSERT(((int)BC_ISEQN^1) == (int)BC_ISNEN);
LJ_STATIC_ASSERT(((int)BC_ISEQP^1) == (int)BC_ISNEP);
LJ_STATIC_ASSERT(((int)BC_ISLT^1) == (int)BC_ISGE);
LJ_STATIC_ASSERT(((int)BC_ISLE^1) == (int)BC_ISGT);
LJ_STATIC_ASSERT(((int)BC_ISLT^3) == (int)BC_ISGT);
LJ_STATIC_ASSERT((int)BC_IST-(int)BC_ISTC == (int)BC_ISF-(int)BC_ISFC);
LJ_STATIC_ASSERT((int)BC_CALLT-(int)BC_CALL == (int)BC_CALLMT-(int)BC_CALLM);
LJ_STATIC_ASSERT((int)BC_CALLMT + 1 == (int)BC_CALLT);
LJ_STATIC_ASSERT((int)BC_RETM + 1 == (int)BC_RET);
LJ_STATIC_ASSERT((int)BC_FORL + 1 == (int)BC_IFORL);
LJ_STATIC_ASSERT((int)BC_FORL + 2 == (int)BC_JFORL);
LJ_STATIC_ASSERT((int)BC_ITERL + 1 == (int)BC_IITERL);
LJ_STATIC_ASSERT((int)BC_ITERL + 2 == (int)BC_JITERL);
LJ_STATIC_ASSERT((int)BC_ITRNL + 1 == (int)BC_IITRNL);
LJ_STATIC_ASSERT((int)BC_ITRNL + 2 == (int)BC_JITRNL);
LJ_STATIC_ASSERT((int)BC_LOOP + 1 == (int)BC_ILOOP);
LJ_STATIC_ASSERT((int)BC_LOOP + 2 == (int)BC_JLOOP);
LJ_STATIC_ASSERT((int)BC_FUNCF + 1 == (int)BC_IFUNCF);
LJ_STATIC_ASSERT((int)BC_FUNCF + 2 == (int)BC_JFUNCF);
LJ_STATIC_ASSERT((int)BC_FUNCV + 1 == (int)BC_IFUNCV);
LJ_STATIC_ASSERT((int)BC_FUNCV + 2 == (int)BC_JFUNCV);

/* This solves a circular dependency problem, change as needed. */
#define FF_next_N       4

/* Stack slots used by FORI/FORL, relative to operand A. */
enum {
  FORL_IDX, FORL_STOP, FORL_STEP, FORL_EXT
};

/* Bytecode operand modes. ORDER BCMode */
typedef enum {
  BCMnone, BCMdst, BCMbase, BCMvar, BCMrbase, BCMuv,  /* Mode A must be <= 7 */
  BCMlit, BCMlits, BCMpri, BCMnum, BCMstr, BCMtab, BCMfunc, BCMjump, BCMcdata,
  BCM_max
} BCMode;
#define BCM___          BCMnone

#define bcmode_a(op)    ((BCMode)(lj_bc_mode[op] & 7))
#define bcmode_b(op)    ((BCMode)((lj_bc_mode[op]>>3) & 15))
#define bcmode_c(op)    ((BCMode)((lj_bc_mode[op]>>7) & 15))
#define bcmode_d(op)    bcmode_c(op)
#define bcmode_hasd(op) ((lj_bc_mode[op] & (15<<3)) == (BCMnone<<3))
#define bcmode_mm(op)   ((enum MMS)(lj_bc_mode[op]>>11))

#define BCMODE(name, ma, mb, mc, mm) \
  (BCM##ma|(BCM##mb<<3)|(BCM##mc<<7)|(MM_##mm<<11)),
#define BCMODE_FF       0

static LJ_AINLINE int bc_isret(BCOp op)
{
  return (op == BC_RETM || op == BC_RET || op == BC_RET0 || op == BC_RET1);
}

static LJ_AINLINE int bc_isloop(BCOp op)
{
  return op == BC_FORL || op == BC_ITERL || op == BC_ITRNL || op == BC_LOOP;
}

static LJ_AINLINE int bc_isiloop(BCOp op)
{
  return op == BC_IFORL || op == BC_IITERL || op == BC_IITRNL || op == BC_ILOOP;
}

static LJ_AINLINE int bc_isjloop(BCOp op)
{
  return op == BC_JFORL || op == BC_JITERL || op == BC_JITRNL || op == BC_JLOOP;
}

static LJ_AINLINE int bc_isiterl(BCOp op)
{
  return op == BC_ITERL || op == BC_IITERL || op == BC_JITERL ;
}

static LJ_AINLINE int bc_isitrnl(BCOp op)
{
  return op == BC_ITRNL || op == BC_IITRNL || op == BC_JITRNL;
}

/*
** Macros and functions for generic x2-encoding/decoding bytecode registers.
*/

/* Determines if BCMode of some operand is a subject to x2-encoding/decoding. */
static LJ_AINLINE int isx2_bcm(BCMode m) {
  return m == BCMdst  || m == BCMvar   /* Variable slots */
      || m == BCMbase || m == BCMrbase /* Base slots for stack frames */
      || m == BCMnum; /* Indeces into prototypes' num constant arrays */
}

#define isx2_a(o) isx2_bcm(bcmode_a(o))
#define isx2_b(o) isx2_bcm(bcmode_b(o))
#define isx2_c(o) isx2_bcm(bcmode_c(o))
#define isx2_d(o) isx2_bcm(bcmode_d(o))

/* x2-encodes register r for the opcode op. */
#define x2_encode_op_r(op, r) ((isx2_##r(op) && (r) != NO_REG)? (r) << 1 : (r))

/* x2-encodes register r of the instruction addressed by the pointer ip. */
#define x2_encode_ip_r(ip, r) \
  ((isx2_##r(bc_op_ip(ip)) && (r) != NO_REG)? (r) << 1 : (r))

/* x2-decodes register r of the instruction i. */
#define x2_decode_r(i, r) ((isx2_##r(bc_op(i)) && (r) != NO_REG)? (r) >> 1 : (r))

/*
** Macros and functions to get instruction fields.
*/

/* Returns the opcode of the instruction i. */
#define bc_op(i)     ((BCOp)((i)&0xff))
/* Returns the opcode of the instruction addressed by the pointer ip. */
#define bc_op_ip(ip) ((BCOp)((uint8_t*)(ip))[0])

#define bc_a_raw(i) ((BCReg)(((i)>>8)&0xff))
#define bc_b_raw(i) ((BCReg)((i)>>24))
#define bc_c_raw(i) ((BCReg)(((i)>>16)&0xff))
#define bc_d_raw(i) ((BCReg)((i)>>16))
#define bc_j(i)     ((ptrdiff_t)bc_d(i)-BCBIAS_J)

static LJ_AINLINE BCReg bc_a(BCIns i) {
  BCReg a = bc_a_raw(i); return x2_decode_r(i, a);
}

static LJ_AINLINE BCReg bc_b(BCIns i) {
  BCReg b = bc_b_raw(i); return x2_decode_r(i, b);
}

static LJ_AINLINE BCReg bc_c(BCIns i) {
  BCReg c = bc_c_raw(i); return x2_decode_r(i, c);
}

static LJ_AINLINE BCReg bc_d(BCIns i) {
  BCReg d = bc_d_raw(i); return x2_decode_r(i, d);
}

static LJ_AINLINE const BCIns *bc_target(const BCIns *pc) {
  lua_assert(bcmode_d(bc_op(*pc)) == BCMjump);
  return pc + 1 + bc_j(*pc);
}

/*
** Macros and functions to set instruction fields.
*/

#define setbc_byte(p, x, ofs) \
  ((uint8_t *)(p))[ofs] = (uint8_t)(x)
#define setbc_op(p, x)      setbc_byte(p, (x), 0)
#define setbc_a_raw(p, x)   setbc_byte(p, (x), 1)
#define setbc_b_raw(p, x)   setbc_byte(p, (x), 3)
#define setbc_c_raw(p, x)   setbc_byte(p, (x), 2)
#define setbc_d_raw(p, x)   ((uint16_t *)(p))[1] = (uint16_t)(x)
#define setbc_j(p, x)       setbc_d_raw(p, (BCPos)((int32_t)(x)+BCBIAS_J))

static LJ_AINLINE void setbc_a(BCIns *p, BCReg a) {
  setbc_a_raw(p, x2_encode_ip_r(p, a));
}

static LJ_AINLINE void setbc_b(BCIns *p, BCReg b) {
  setbc_b_raw(p, x2_encode_ip_r(p, b));
}

static LJ_AINLINE void setbc_c(BCIns *p, BCReg c) {
  setbc_c_raw(p, x2_encode_ip_r(p, c));
}

static LJ_AINLINE void setbc_d(BCIns *p, BCReg d) {
  setbc_d_raw(p, x2_encode_ip_r(p, d));
}

/*
** Macros and functions to compose instructions.
*/

#define BCINS_ABC_raw(o, a, b, c) \
  (((BCIns)(o))|((BCIns)(a)<<8)|((BCIns)(b)<<24)|((BCIns)(c)<<16))
#define BCINS_AD_raw(o, a, d) \
  (((BCIns)(o))|((BCIns)(a)<<8)|((BCIns)(d)<<16))
#define BCINS_AJ_raw(o, a, j) BCINS_AD_raw(o, a, (BCPos)((int32_t)(j)+BCBIAS_J))

static LJ_AINLINE BCIns BCINS_ABC(BCOp o, BCReg a, BCReg b, BCReg c) {
  return BCINS_ABC_raw(o,
    x2_encode_op_r(o, a),
    x2_encode_op_r(o, b),
    x2_encode_op_r(o, c)
  );
}

static LJ_AINLINE BCIns BCINS_AD(BCOp o, BCReg a, BCReg d) {
  return BCINS_AD_raw(o, x2_encode_op_r(o, a), x2_encode_op_r(o, d));
}

static LJ_AINLINE BCIns BCINS_AJ(BCOp o, BCReg a, BCReg j) {
  return BCINS_AJ_raw(o, x2_encode_op_r(o, a), j);
}

#endif
