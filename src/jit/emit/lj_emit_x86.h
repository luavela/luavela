/*
 * x86/x64 instruction emitter.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "jit/lj_jit.h"

/* -- Emit basic instructions --------------------------------------------- */

#define MODRM(mode, r1, r2) ((MCode)((mode)+(((r1)&7)<<3)+((r2)&7)))
#define SIB(scale, index, base) MODRM((scale), (index), (base))

#define REXRB(p, rr, rb) \
    { MCode rex = 0x40 + (((rr)>>1)&4) + (((rb)>>3)&1); \
      if (rex != 0x40) *--(p) = rex; }
#define FORCE_REX               0x200
#define REX_64                  (FORCE_REX|0x080000)

#define emit_i8(as, i)          (*--(as)->mcp = (MCode)(i))
#define emit_i32(as, i)         (*(int32_t *)((as)->mcp-4) = (i), (as)->mcp -= 4)
#define emit_u32(as, u)         (*(uint32_t *)((as)->mcp-4) = (u), (as)->mcp -= 4)

#define emit_x87op(as, xo) \
  (*(uint16_t *)((as)->mcp-2) = (uint16_t)(xo), (as)->mcp -= 2)

#define ptr2imm64(ptr) ((uint64_t)(uintptr_t)(void *)(ptr))

/* op */
MCode *emit_op(x86Op xo, Reg rr, Reg rb, Reg rx, MCode *p, int delta);

/* op + modrm */
#define emit_opm(xo, mode, rr, rb, p, delta) \
  ((p)[(delta)-1] = MODRM((mode), (rr), (rb)), \
   emit_op((xo), (rr), (rb), 0, (p), (delta)))

/* op + modrm + sib */
#define emit_opmx(xo, mode, scale, rr, rb, rx, p) \
  ((p)[-1] = MODRM((scale), (rx), (rb)), \
   (p)[-2] = MODRM((mode), (rr), RID_ESP), \
   emit_op((xo), (rr), (rb), (rx), (p), -1))

/* op r1, r2 */
void emit_rr(ASMState *as, x86Op xo, Reg r1, Reg r2);

#define ptr2addr(p)     (i32ptr((p)))                  /* Low dword of ptr. */

/* mov r, imm64
** Forced 8-byte version.
*/
void emit_loadu64_(ASMState *, Reg, uint64_t);

void emit_loada(ASMState *as, Reg r, const void *addr);

/* op r, [base+ofs] */
void emit_rmro(ASMState *as, x86Op xo, Reg rr, Reg rb, int32_t ofs);

/* op r, [addr] */
/* Assembled as two instructions:
**   mov RID_INTERNAL, addr
**   op r, [RID_INTERNAL]
*/
void emit_rma(ASMState *as, x86Op xo, Reg rr, const void *addr);

/* op r, i */
void emit_gri(ASMState *as, x86Group xg, Reg rb, int32_t i);

/* cmp [base+ofs], imm64 */
void emit_cmp_bo_imm64(ASMState *as, Reg base, int32_t ofs, uint64_t imm64);

#define emit_shifti(as, xg, r, i) \
  (emit_i8(as, (i)), emit_rr(as, XO_SHIFTi, (Reg)(xg), (r)))

/* op r, rm/mrm */
void emit_mrm(ASMState *as, x86Op xo, Reg rr, Reg rb);

/* op rm/mrm, i */
void emit_gmrmi(ASMState *as, x86Group xg, Reg rb, int32_t i);

/* -- Emit loads/stores --------------------------------------------------- */

/* mov [base+ofs], i */
void emit_movmroi(ASMState *as, Reg base, int32_t ofs, int32_t i);

/* mov [base+ofs], imm64 */
void emit_movmroi64(ASMState *as, Reg base, int32_t ofs, int64_t imm64);

/* mov [base+ofs], r */
#define emit_movtomro(as, r, base, ofs) \
  emit_rmro(as, XO_MOVto, (r), (base), (ofs))

/* mov r, i / xor r, r */
void emit_loadi(ASMState *as, Reg r, int32_t i);

/* mov r, imm64 or shorter 32 bit extended load. */
void emit_loadu64(ASMState *as, Reg r, uint64_t imm64);

/* movsd r, [&tv->n] / xorps r, r */
void emit_loadn(ASMState *as, Reg r, const TValue *tv);

/* -- Emit generic operations --------------------------------------------- */

/* Use 64 bit operations to handle 64 bit IR types. */
#define REX_64IR(ir, r)     ((r) + (irt_is64((ir)->t) ? REX_64 : 0))

/* movdqu xmm, [r64] */
void emit_mxmmm128(ASMState *as, uint8_t xmmr, uint8_t gpr);

/* movdqu [r64], xmm */
void emit_mm128xmm(ASMState *as, uint8_t gpr, uint8_t xmmr);

/* Generic move between two regs. */
void emit_movrr(ASMState *as, IRIns *ir, Reg dst, Reg src);

/* Generic load of register from stack slot. */
void emit_spload(ASMState *as, IRIns *ir, Reg r, int32_t ofs);

/* Generic store of register to stack slot. */
void emit_spstore(ASMState *as, IRIns *ir, Reg r, int32_t ofs);

/* Add offset to pointer. */
void emit_addptr(ASMState *as, Reg r, int32_t ofs);

#define emit_spsub(as, ofs) emit_addptr(as, RID_ESP|REX_64, -(ofs))

/* Prefer rematerialization of BASE/L from global_State over spills. */
#define emit_canremat(ref) ((ref) <= REF_BASE)
