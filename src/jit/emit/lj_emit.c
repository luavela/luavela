/*
 * Machine code emitter.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "jit/emit/lj_emit_x86.h"
#include "jit/emit/uj_emit_sse2.h"

/* op */
MCode *emit_op(x86Op xo, Reg rr, Reg rb, Reg rx, MCode *p, int delta) {
  int n = (int8_t)xo;
  if (__builtin_constant_p(xo) && n == -2) {
    p[delta-2] = (MCode)(xo >> 24);
  } else if (__builtin_constant_p(xo) && n == -3) {
    *(uint16_t *)(p+delta-3) = (uint16_t)(xo >> 16);
  } else {
    *(uint32_t *)(p+delta-5) = (uint32_t)xo;
  }
  p += n + delta;
  {
    uint32_t rex = 0x40 + ((rr>>1)&(4+(FORCE_REX>>1)))+((rx>>2)&2)+((rb>>3)&1);
    if (rex != 0x40) {
      rex |= (rr >> 16);
      if (n == -4) {
        *p = (MCode)rex; rex = (MCode)(xo >> 8);
      } else if ((xo & 0xffffff) == 0x6600fd) {
        *p = (MCode)rex; rex = 0x66;
      }
      *--p = (MCode)rex;
    }
  }
  return p;
}

/* op r1, r2 */
void emit_rr(ASMState *as, x86Op xo, Reg r1, Reg r2) {
  MCode *p = as->mcp;
  as->mcp = emit_opm(xo, XM_REG, r1, r2, p, 0);
}

/* mov r, imm64
** Forced 8-byte version.
*/
void emit_loadu64_(ASMState *as, Reg r, uint64_t imm64) {
  MCode *p = as->mcp;
  *(uint64_t *)(p-8) = imm64;
  p[-9] = (MCode)(XI_MOVri+(r&7));
  p[-10] = 0x48 + ((r>>3)&1);
  p -= 10;
  as->mcp = p;
}

void emit_loada(ASMState *as, Reg r, const void *addr) {
  emit_loadu64_(as, r, ptr2imm64(addr));
}

/* op r, [base+ofs] */
void emit_rmro(ASMState *as, x86Op xo, Reg rr, Reg rb, int32_t ofs) {
  MCode *p = as->mcp;
  x86Mode mode;
  if (ra_hasreg(rb)) {
    if (ofs == 0 && (rb&7) != RID_EBP) {
      mode = XM_OFS0;
    } else if (checki8(ofs)) {
      *--p = (MCode)ofs;
      mode = XM_OFS8;
    } else {
      p -= 4;
      *(int32_t *)p = ofs;
      mode = XM_OFS32;
    }
    if ((rb&7) == RID_ESP) {
      *--p = MODRM(XM_SCALE1, RID_ESP, RID_ESP);
    }
  } else {
    *(int32_t *)(p-4) = ofs;
    p[-5] = MODRM(XM_SCALE1, RID_ESP, RID_EBP);
    p -= 5;
    rb = RID_ESP;
    mode = XM_OFS0;
  }
  as->mcp = emit_opm(xo, mode, rr, rb, p, 0);
}

/* op r, [addr] */
/* Assembled as two instructions:
**   mov RID_INTERNAL, addr
**   op r, [RID_INTERNAL]
*/
void emit_rma(ASMState *as, x86Op xo, Reg rr, const void *addr)
{
  emit_rmro(as, xo, rr, RID_INTERNAL, 0);
  emit_loada(as, RID_INTERNAL, addr);
}

/* op r, i */
void emit_gri(ASMState *as, x86Group xg, Reg rb, int32_t i) {
  MCode *p = as->mcp;
  x86Op xo;
  if (checki8(i)) {
    *--p = (MCode)i;
    xo = XG_TOXOi8(xg);
  } else {
    p -= 4;
    *(int32_t *)p = i;
    xo = XG_TOXOi(xg);
  }
  as->mcp = emit_opm(xo, XM_REG, (Reg)(xg & 7) | (rb & REX_64), rb, p, 0);
}

/* cmp [base+ofs], imm64 */
void emit_cmp_bo_imm64(ASMState *as, Reg base, int32_t ofs,
                              uint64_t imm64) {
  emit_rmro(as, XO_CMP, RID_INTERNAL|REX_64, base, ofs);
  emit_loadu64_(as, RID_INTERNAL, imm64);
}

/* op r, rm/mrm */
void emit_mrm(ASMState *as, x86Op xo, Reg rr, Reg rb) {
  MCode *p = as->mcp;
  x86Mode mode = XM_REG;
  if (rb == RID_MRM) {
    rb = as->mrm.base;
    if (rb == RID_NONE) {
      rb = RID_EBP;
      mode = XM_OFS0;
      p -= 4;
      *(int32_t *)p = as->mrm.ofs;
      if (as->mrm.idx != RID_NONE) {
        goto mrmidx;
      }
      *--p = MODRM(XM_SCALE1, RID_ESP, RID_EBP);
      rb = RID_ESP;
    } else {
      if (as->mrm.ofs == 0 && (rb&7) != RID_EBP) {
        mode = XM_OFS0;
      } else if (checki8(as->mrm.ofs)) {
        *--p = (MCode)as->mrm.ofs;
        mode = XM_OFS8;
      } else {
        p -= 4;
        *(int32_t *)p = as->mrm.ofs;
        mode = XM_OFS32;
      }
      if (as->mrm.idx != RID_NONE) {
      mrmidx:
        as->mcp = emit_opmx(xo, mode, as->mrm.scale, rr, rb, as->mrm.idx, p);
        return;
      }
      if ((rb&7) == RID_ESP) {
        *--p = MODRM(XM_SCALE1, RID_ESP, RID_ESP);
      }
    }
  }
  as->mcp = emit_opm(xo, mode, rr, rb, p, 0);
}

/* op rm/mrm, i */
void emit_gmrmi(ASMState *as, x86Group xg, Reg rb, int32_t i) {
  x86Op xo;
  if (checki8(i)) {
    emit_i8(as, i);
    xo = XG_TOXOi8(xg);
  } else {
    emit_i32(as, i);
    xo = XG_TOXOi(xg);
  }
  emit_mrm(as, xo, (Reg)(xg & 7) | (rb & REX_64), (rb & ~REX_64));
}

/* -- Emit loads/stores --------------------------------------------------- */

/* mov [base+ofs], i */
void emit_movmroi(ASMState *as, Reg base, int32_t ofs, int32_t i) {
  emit_i32(as, i);
  emit_rmro(as, XO_MOVmi, 0, base, ofs);
}

/* mov [base+ofs], imm64 */
void emit_movmroi64(ASMState *as, Reg base, int32_t ofs, int64_t imm64) {
  emit_movmroi(as, base, ofs, (int32_t)imm64);
  emit_movmroi(as, base, ofs+4, (int32_t)(imm64 >> 32));
}

/* mov r, i / xor r, r */
void emit_loadi(ASMState *as, Reg r, int32_t i) {
  /* XOR r,r is shorter, but modifies the flags. This is bad for HIOP/jcc. */
  if (i == 0 && !((*as->mcp == 0x0f && (as->mcp[1] & 0xf0) == XI_JCCn) ||
		  (*as->mcp & 0xf0) == XI_JCCs)) {
    emit_rr(as, XO_ARITH(XOg_XOR), r, r);
  } else {
    MCode *p = as->mcp;
    *(int32_t *)(p-4) = i;
    p[-5] = (MCode)(XI_MOVri+(r&7));
    p -= 5;
    REXRB(p, 0, r);
    as->mcp = p;
  }
}

/* mov r, imm64 or shorter 32 bit extended load. */
void emit_loadu64(ASMState *as, Reg r, uint64_t imm64) {
  if (checku32(imm64)) {  /* 32 bit load clears upper 32 bits. */
    emit_loadi(as, r, (int32_t)imm64);
  } else if (checki32((int64_t)imm64)) {  /* Sign-extended 32 bit load. */
    MCode *p = as->mcp;
    *(int32_t *)(p-4) = (int32_t)imm64;
    as->mcp = emit_opm(XO_MOVmi, XM_REG, REX_64, r, p, -4);
  } else {  /* Full-size 64 bit load. */
    emit_loadu64_(as, r, imm64);
  }
}

/* movsd r, [&tv->n] / xorps r, r */
void emit_loadn(ASMState *as, Reg r, const TValue *tv) {
  if (tvispzero(tv)) { /* Use xor only for +0. */
    emit_rr(as, XO_XORPS, r, r);
  } else {
    emit_rma(as, XO_MOVSD, r, &tv->n);
  }
}

/* -- Emit generic operations --------------------------------------------- */

#define LOCAL_MC_SIZE 16

static void emit_save(ASMState *as, const uint8_t *mc, size_t n) {
  lua_assert(n > 0 && n <= LOCAL_MC_SIZE);
  memcpy(as->mcp - n, mc, n);
  as->mcp -= n;
}

/* movdqu xmm, [r64] */
void emit_mxmmm128(ASMState *as, uint8_t xmmr, uint8_t gpr)
{
  uint8_t mc[LOCAL_MC_SIZE] = {0};
  size_t n = uj_emit_movxmmrm(mc, xmmr, gpr);

  emit_save(as, mc, n);
}

/* movdqu [r64], xmm */
void emit_mm128xmm(ASMState *as, uint8_t gpr, uint8_t xmmr)
{
  uint8_t mc[LOCAL_MC_SIZE] = {0};
  size_t n = uj_emit_movrmxmm(mc, gpr, xmmr);

  emit_save(as, mc, n);
}

/* movdqu xmm, XMMWORD PTR [rsp + ofs] */
static void emit_spload_xmm(ASMState *as, Reg xmm, int32_t ofs)
{
  uint8_t mc[LOCAL_MC_SIZE] = {0};
  size_t n = uj_emit_spload_xmm(mc, xmm, ofs);

  emit_save(as, mc, n);
}

/* movdqu XMMWORD PTR [rsp + ofs], xmm */
void emit_spstore_xmm(ASMState *as, Reg xmm, int32_t ofs)
{
  uint8_t mc[LOCAL_MC_SIZE] = {0};
  size_t n = uj_emit_spstore_xmm(mc, xmm, ofs);

  emit_save(as, mc, n);
}

/* Generic move between two regs. */
void emit_movrr(ASMState *as, IRIns *ir, Reg dst, Reg src) {
  UNUSED(ir);
  if (dst < RID_MAX_GPR) {
    emit_rr(as, XO_MOV, REX_64IR(ir, dst), src);
  } else {
    emit_rr(as, XO_MOVAPS, dst, src);
  }
}

/* Generic load of register from stack slot. */
void emit_spload(ASMState *as, IRIns *ir, Reg r, int32_t ofs) {
  if (r < RID_MAX_GPR) {
    emit_rmro(as, XO_MOV, REX_64IR(ir, r), RID_ESP, ofs);
  } else {
    if (irt_istval(ir->t)) {
      lua_assert(ir_hashint(ir, IRH_MOVTV));
      emit_spload_xmm(as, r, ofs);
    } else {
      emit_rmro(as, irt_isnum(ir->t) ? XO_MOVSD : XO_MOVSS, r, RID_ESP, ofs);
    }
  }
}

/* Generic store of register to stack slot. */
void emit_spstore(ASMState *as, IRIns *ir, Reg r, int32_t ofs) {
  if (r < RID_MAX_GPR) {
    emit_rmro(as, XO_MOVto, REX_64IR(ir, r), RID_ESP, ofs);
  } else {
    if (irt_istval(ir->t)) {
      lua_assert(ir_hashint(ir, IRH_MOVTV));
      emit_spstore_xmm(as, r, ofs);
    } else {
      emit_rmro(as, irt_isnum(ir->t) ? XO_MOVSDto : XO_MOVSSto, r, RID_ESP, ofs);
    }
  }
}

/* Add offset to pointer. */
void emit_addptr(ASMState *as, Reg r, int32_t ofs) {
  if (ofs) { emit_gri(as, XG_ARITHi(XOg_ADD), r, ofs); }
}
