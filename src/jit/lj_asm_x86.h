/*
 * x86/x64 IR assembler (SSA IR -> machine code).
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include <math.h>
#include "jit/opt_x86/opt_x86_iface.h"

/* -- Guard handling ------------------------------------------------------ */

/* Generate an exit stub group at the bottom of the reserved MCode memory.
**
** This mcode is assembled manually, so it's full of magic numbers and
** strange aliases. The main reason why regular emitter is not used there,
** is because emitter is tightly bound to as->mcp (which is not the case
** here), plus it's surprisingly convenient to emit code in the regular
** top-to-bottom direction.
** Anyway, this code snippet consists of two major blocks. The first one
** pushes particular exit number to the host stack, the second pushes
** DISPATCH register value to be restored by the interpreter and jumps
** to the exit handler (see definition in vm_x86.dasc).
*/
static MCode *asm_exitstub_gen(ASMState *as, ExitNo group) {
  ExitNo i, groupofs = (group*EXITSTUBS_PER_GROUP) & 0xff;
  MCode *mxp = as->mcbot;
  MCode *mxpstart = mxp;

  /* -- Ensure mcode space for traceno snippet. ----------------------------- */
  if (mxp + (2+2)*EXITSTUBS_PER_GROUP >= as->mctop) { asm_mclimit(as); }

  /* Push low byte of exitno for each exit stub. */
  *mxp++ = XI_PUSHi8; *mxp++ = (MCode)groupofs;
  for (i = 1; i < EXITSTUBS_PER_GROUP; i++) {
    *mxp++ = XI_JMPs; *mxp++ = (MCode)((2+2)*(EXITSTUBS_PER_GROUP - i) - 2);
    *mxp++ = XI_PUSHi8; *mxp++ = (MCode)(groupofs + i);
  }
  /* Push the high byte of the exitno for each exit stub group. */
  *mxp++ = XI_PUSHi8; *mxp++ = (MCode)((group*EXITSTUBS_PER_GROUP)>>8);

  /* -- Ensure mcode space for DISPATCH snippet. ---------------------------- */
  if (mxp + 15 >= as->mctop) { asm_mclimit(as); }

  /* Store DISPATCH at original stack slot 0. Account for the two push
  ** ops (lo-word and hi-word of exitno) above.
  */

  /* mov r11, imm64 */
  *mxp++ = 0x49; /* REX.W + REX.B */
  *mxp++ = XI_MOVri + (RID_INTERNAL & 0x7);
  *(uint64_t *)mxp = (uint64_t)(void *)(J2GG(as->J)->dispatch); mxp += 8;

  /* mov [rsp+0x10], r11 */
  *mxp++ = 0x4c; /* REX.W + REX.R */
  *mxp++ = 0x89; /* MOV opcode */
  *mxp++ = 0x5c; /* ModRM: mod=01 reg=011 rm=100 */
  *mxp++ = 0x24; /* SIB: scale=00 index=100 base=100 */
  *mxp++ = 0x10; /* disp8 */

  /* -- Ensure mcode space for exit jump. ----------------------------------- */
  if ( mxp + 13 >= as->mctop) { asm_mclimit(as); }

  /* Jump to exit handler which fills in the ExitState. */

  /* mov r11, @lj_vm_exit_handler */
  *mxp++ = 0x49; /* REX.W + REX.B */
  *mxp++ = 0xb8 + (RID_INTERNAL & 0x7); /* MOV ro io */
UJ_PEDANTIC_OFF /* casting a function ptr to void* */
  *(uint64_t *)mxp = (uint64_t)(void *)lj_vm_exit_handler; mxp += 8;
UJ_PEDANTIC_ON

  /* jmp r11 */
  *mxp++ = 0x41; /* REX.B */
  *mxp++ = 0xff; /* JMP /4 */
  *mxp++ = MODRM(XM_REG, 4, RID_INTERNAL & 0x7);

  /* -- Commit the code for this group (even if assembly fails later on). --- */
  lj_mcode_commitbot(as->J, mxp);
  as->mcbot = mxp;
  as->mclim = as->mcbot + MCLIM_REDZONE;
  return mxpstart;
}

/* Setup all needed exit stubs. */
static void asm_exitstub_setup(ASMState *as, ExitNo nexits) {
  ExitNo i;
  if (nexits >= EXITSTUBS_PER_GROUP*LJ_MAX_EXITSTUBGR) {
    lj_trace_err(as->J, LJ_TRERR_SNAPOV);
  }
  for (i = 0; i < (nexits+EXITSTUBS_PER_GROUP-1)/EXITSTUBS_PER_GROUP; i++) {
    if (as->J->exitstubgroup[i] == NULL) {
      as->J->exitstubgroup[i] = asm_exitstub_gen(as, i);
    }
  }
}

/* Emit conditional branch to exit for guard.
** It's important to emit this *after* all registers have been allocated,
** because rematerializations may invalidate the flags.
*/
static void asm_guardcc(ASMState *as, int cc) {
  MCode *target = exitstub_addr(as->J, as->snapno);
  MCode *p = as->mcp;
  if (LJ_UNLIKELY(p == as->invmcp)) {
    as->loopinv = 1;
    *(int32_t *)(p+1) = uj_emit_rel32(p+5, target);
    target = p;
    cc ^= 1;
    if (as->realign) {
      uj_emit_jccs(as, cc, target);
      return;
    }
  }
  uj_emit_jccn(as, cc, target);
}

static LJ_AINLINE int asm_needs_movtv(const ASMState *as, const IRIns *ir)
{
  if (!(as->flags & JIT_F_OPT_MOVTV)) /* the optimization is turned off */
    return 0;

  if (!ir_hashint(ir, IRH_MOVTV)) /* ir is not touched by the optimization */
    return 0;

  return 1;
}

/*
 * For each IR that can return IRT_TAB, assemble an extra "no metatable" guard
 * if the IR contains an appropriate set of hints. Affected IRs are:
 * * All IR_*LOAD, except IR_XLOAD.
 * * All IR_CALL* in case a called function returns a GCtab *. However,
 *   currently there are no calls that return tables with metatables set.
 * * TNEW, TDUP: These are implemented via IR_CALL*, so see above.
 * All cases where a call to this function can be omitted
 * are annotated with a SKIPNOMT comment. See also MOVTV implementation.
 */
static void asm_guard_nomt(ASMState *as, const IRIns *ir, Reg dest) {
  lua_assert(irt_istab(ir->t));

  /*
   * The guard will be assembled elsewhere. Or, if it was eliminated
   * by some other optimization, it is excessive anyway.
   */
  if (ir_hashint(ir, IRH_TAB_NOMETAGUARD))
    return;

  asm_guardcc(as, CC_NE);
  emit_i8(as, 0);
  emit_rmro(as, XO_ARITHi8, XOg_CMP | REX_64, dest, offsetof(GCtab, metatable));
}

/* -- Memory operand validation ------------------------------------------- */

#ifdef NDEBUG
static void asm_assert_abase(ASMState *as, IRRef ref) {
  UNUSED(as);
  UNUSED(ref);
}
#else
static void asm_assert_abase(ASMState *as, IRRef ref) {
  IRIns *irb = IR(ref);
  if (irb->o == IR_FLOAD) {
    /* We can avoid the FLOAD of t->array for colocated arrays. */
    lua_assert(irb->op2 == IRFL_TAB_ARRAY);
  } else if (irb->o == IR_ADD && irref_isk(irb->op2)) {
    /* vararg load: Ensure that displacement is of proper size. */
    lua_assert(checki32(IR(irb->op2)->i));
  }
}
#endif /* NDEBUG */

/* -- Memory operand fusion ----------------------------------------------- */

/* Check if a reference is a signed 32 bit constant. */
static int asm_isk32(ASMState *as, IRRef ref, int32_t *k) {
  if (irref_isk(ref)) {
    IRIns *ir = IR(ref);
    if (irt_isaddr(ir->t)) { return 0; }
    if (ir->o != IR_KINT64) {
      *k = ir->i;
      return 1;
    } else if (checki32((int64_t)rawV(ir_kint64(ir)))) {
      *k = (int32_t)rawV(ir_kint64(ir));
      return 1;
    }
  }
  return 0;
}

/* Fuse array/hash/upvalue reference into memory operand.
** Caveat: this may allocate GPRs for the base/idx registers. Be sure to
** pass the final allow mask, excluding any GPRs used for other inputs.
** In particular: 2-operand GPR instructions need to call ra_dest() first!
*/
static void asm_fuseahuref(ASMState *as, const IRIns *ir, RegSet allow) {
  as->mrm.base = (uint8_t)ra_alloc1(as, ir->op1, allow);
  as->mrm.ofs = 0;
  as->mrm.idx = RID_NONE;
  if (ir->o == IR_HKLOAD)
    as->mrm.ofs += offsetof(Node, key);
}

/* Fuse FLOAD/FREF reference into memory operand. */
static void asm_fusefref(ASMState *as, IRIns *ir, RegSet allow) {
  lua_assert(ir->o == IR_FLOAD || ir->o == IR_FREF);
  as->mrm.ofs = field_ofs[ir->op2];
  as->mrm.idx = RID_NONE;
  as->mrm.base = (uint8_t)ra_alloc1(as, ir->op1, allow);
}

/* Fuse string reference into memory operand. */
static void asm_fusestrref(ASMState *as, IRIns *ir, RegSet allow) {
  IRIns *irr;
  lua_assert(ir->o == IR_STRREF);
  as->mrm.base = as->mrm.idx = RID_NONE;
  as->mrm.scale = XM_SCALE1;
  as->mrm.ofs = sizeof(GCstr);
  //if (irref_isk(ir->op1)) {
  //  as->mrm.ofs += IR(ir->op1)->i;
  //} else {
    Reg r = ra_alloc1(as, ir->op1, allow);
    rset_clear(allow, r);
    as->mrm.base = (uint8_t)r;
  //}
  irr = IR(ir->op2);
  if (irref_isk(ir->op2)) {
    lua_assert(checki32(irr->i));
    as->mrm.ofs += irr->i;
  } else {
    Reg r;
    r = ra_alloc1(as, ir->op2, allow);
    if (as->mrm.base == RID_NONE) {
      as->mrm.base = (uint8_t)r;
    } else {
      as->mrm.idx = (uint8_t)r;
    }
  }
}

static void asm_fusexref(ASMState *as, IRRef ref, RegSet allow) {
  as->mrm.idx = RID_NONE;
  as->mrm.ofs = 0;
  as->mrm.base = (uint8_t)ra_alloc1(as, ref, allow);
}

/* Fuse load into memory operand. */
static Reg asm_fuseload(ASMState *as, IRRef ref, RegSet allow) {
  IRIns *ir = IR(ref);
  if (ra_hasreg(ir->r)) {
    if (allow != RSET_EMPTY) {  /* Fast path. */
      ra_noweak(as, ir->r);
      return ir->r;
    }
    /* Force a spill if only memory operands are allowed (asm_x87load). */
  fusespill:
    as->mrm.base = RID_ESP;
    as->mrm.ofs = ra_spill(as, ir);
    as->mrm.idx = RID_NONE;
    return RID_MRM;
  }

  if (!(as->freeset & allow) && !emit_canremat(ref) &&
       (allow == RSET_EMPTY || ra_hasspill(ir->s) || iscrossref(as, ref))
     ) {
    goto fusespill;
   }

  return ra_allocref(as, ref, allow);
}

/* Don't fuse a 32 bit load into a 64 bit operation. */
static Reg asm_fuseloadm(ASMState *as, IRRef ref, RegSet allow, int is64) {
  if (is64 && !irt_is64(IR(ref)->t)) {
    return ra_alloc1(as, ref, allow);
  }
  return asm_fuseload(as, ref, allow);
}

/* -- Calls --------------------------------------------------------------- */

/* Count the required number of stack slots for a call. */
static int asm_count_call_slots(ASMState *as, const CCallInfo *ci, IRRef *args) {
  uint32_t i, nargs = CCI_NARGS(ci);
  int nslots = 0;
  int ngpr = REGARG_NUMGPR, nfpr = REGARG_NUMFPR;
  for (i = 0; i < nargs; i++) {
    if (args[i] && irt_isfp(IR(args[i])->t)) {
      if (nfpr > 0) nfpr--; else nslots += 2;
    } else {
      if (ngpr > 0) ngpr--; else nslots += 2;
    }
  }
  return nslots;
}

/* Generate a call to a C function with nargs arguments. */
static void asm_gencall_nargs(ASMState *as, const CCallInfo *ci,
                              const IRRef *args, size_t nargs) {
  uint32_t n;
  int32_t ofs = STACKARG_OFS;
  uint32_t gprs = REGARG_GPRS;
  Reg fpr = REGARG_FIRSTFPR;
  MCode *patchnfpr = NULL;

  lua_assert(nargs == CCI_NARGS(ci));

UJ_PEDANTIC_OFF /* casting a function ptr to void* */
  if ((void *)ci->func) {
    uj_emit_call(as, ci->func);
  }
UJ_PEDANTIC_ON
  if ((ci->flags & CCI_VARARG)) {  /* Special handling for vararg calls. */
    patchnfpr = --as->mcp;  /* Indicate number of used FPRs in register al. */
    *--as->mcp = XI_MOVrib | RID_EAX;
  }
  for (n = 0; n < nargs; n++) {  /* Setup args. */
    IRRef ref = args[n];
    IRIns *ir = IR(ref);
    Reg r;
    /* POSIX/x64 argument registers are used in order of appearance. */
    if (irt_isfp(ir->t)) {
      r = fpr <= REGARG_LASTFPR ? fpr++ : 0;
    } else {
      r = gprs & 31; gprs >>= 5;
    }
    if (r) {  /* Argument is in a register. */
      if (r < RID_MAX_GPR && ref < ASMREF_TMP1) {
        if (ir->o == IR_KINT64) {
          emit_loadu64(as, r, rawV(ir_kint64(ir)));
        } else {
          //emit_loadi(as, r, ir->i);
          emit_loadu64(as, r, (uint64_t)ir->i);
        }
      } else {
        lua_assert(rset_test(as->freeset, r));  /* Must have been evicted. */
        if (ra_hasreg(ir->r)) {
          ra_noweak(as, ir->r);
          emit_movrr(as, ir, r, ir->r);
        } else {
          ra_allocref(as, ref, RID2RSET(r));
        }
      }
    } else if (irt_isfp(ir->t)) {  /* FP argument is on stack. */
      lua_assert(!(irt_isfloat(ir->t) && irref_isk(ref)));  /* No float k. */
      r = ra_alloc1(as, ref, RSET_FPR);
      emit_rmro(as, irt_isnum(ir->t) ? XO_MOVSDto : XO_MOVSSto,
          r, RID_ESP, ofs);
      ofs += 8;
    } else {  /* Non-FP argument is on stack. */
      r = ra_alloc1(as, ref, RSET_GPR);
      emit_movtomro(as, REX_64 + r, RID_ESP, ofs);
      ofs += sizeof(intptr_t);
    }
    checkmclim(as);
  }
  if (patchnfpr) *patchnfpr = fpr - REGARG_FIRSTFPR;
}

/* Setup result reg/sp for call. Evict scratch regs. */
static void asm_setupresult(ASMState *as, IRIns *ir, const CCallInfo *ci) {
  RegSet drop = RSET_SCRATCH;
  if ((ci->flags & CCI_NOFPRCLOBBER)) {
    drop &= ~RSET_FPR;
  }
  if (ra_hasreg(ir->r)) {
    rset_clear(drop, ir->r);  /* Dest reg handled below. */
  }
  ra_evictset(as, drop);  /* Evictions must be performed first. */
  if (ra_used(ir)) {
    if (irt_isfp(ir->t)) {
      int32_t ofs = sps_scale(ir->s);  /* Use spill slot or temp slots. */
      if ((ci->flags & CCI_CASTU64)) {
        Reg dest = ir->r;
        if (ra_hasreg(dest)) {
          ra_free(as, dest);
          ra_modified(as, dest);
          emit_rr(as, XO_MOVD, dest|REX_64, RID_RET);  /* Really MOVQ. */
        }
        if (ofs) { emit_movtomro(as, RID_RET|REX_64, RID_ESP, ofs); }
      } else {
        ra_destreg(as, ir, RID_FPRET);
      }
    } else {
      lua_assert(!irt_ispri(ir->t));
      ra_destreg(as, ir, RID_RET);
    }
  }
}

static void asm_call(ASMState *as, IRIns *ir) {
  IRRef args[CCI_NARGS_MAX];
  const CCallInfo *ci = &lj_ir_callinfo[ir->op2];

  /* SKIPNOMT: Neither native call returns a table with a metatable set. */
  if (ci->flags & CCI_ALLOC) {
    as->gcsteps++;
  }
  asm_collectargs(as, ir, ci, args);
  asm_setupresult(as, ir, ci);
  asm_gencall_nargs(as, ci, args, CCI_NARGS(ci));
}

/* Return a constant function pointer or NULL for indirect calls. */
static void *asm_callx_func(ASMState *as, IRIns *irf, IRRef func) {
  if (irref_isk(func)) {
    MCode *p;
    if (irf->o == IR_KINT64) {
      p = (MCode *)(void *)rawV(ir_k64(irf));
    } else {
      p = (MCode *)(void *)(uintptr_t)(uint32_t)irf->i;
    }
    if (p - as->mcp == (int32_t)(p - as->mcp)) {
      return p;  /* Call target is still in +-2GB range. */
    }
    /* Avoid the indirect case of uj_emit_call(). Try to hoist func addr. */
  }
  return NULL;
}

static void asm_callx(ASMState *as, IRIns *ir) {
  IRRef args[CCI_NARGS_MAX*2];
  CCallInfo ci;
  IRRef func;
  IRIns *irf;
  ci.flags = asm_callx_flags(as, ir);
  asm_collectargs(as, ir, &ci, args);
  asm_setupresult(as, ir, &ci);
  func = ir->op2; irf = IR(func);
  if (irf->o == IR_CARG) { func = irf->op1; irf = IR(func); }
UJ_PEDANTIC_OFF /* casting void* to a function ptr (and vice versa) */
  ci.func = (ASMFunction)asm_callx_func(as, irf, func);
  if (!(void *)ci.func) {
UJ_PEDANTIC_ON
    /* Use a (hoistable) non-scratch register for indirect calls. */
    RegSet allow = (RSET_GPR & ~RSET_SCRATCH);
    Reg r = ra_alloc1(as, func, allow);
    emit_rr(as, XO_GROUP5, XOg_CALL, r);
  }
  asm_gencall_nargs(as, &ci, args, CCI_NARGS(&ci));
}

/* -- Returns ------------------------------------------------------------- */

/* Helper that emits `test rax, rax` and a consequent side exit based on `cc`
** value. Assumes asm_snap_prep() already done -- it is enough to emit
** corresponding IR as a guard to ensure this.
*/
static void asm_test_retval_zero(ASMState *as, x86CC cc) {
  lua_assert(cc == CC_E || cc == CC_NE);
  asm_guardcc(as, cc);
  emit_rr(as, XO_TEST, RID_RET|REX_64, RID_RET);
}

/* Return to lower frame. Guard that it goes to the right spot. */
static void asm_retf(ASMState *as, IRIns *ir) {
  Reg base = ra_alloc1(as, REF_BASE, RSET_GPR);
  void *pc = ir_kptr(IR(ir->op2));
  int32_t delta = 1+bc_a(*((const BCIns *)pc - 1));
  as->topslot -= (BCReg)delta;
  if ((int32_t)as->topslot < 0) { as->topslot = 0; }
  irt_setmark(IR(REF_BASE)->t);  /* Children must not coalesce with BASE reg. */
  emit_setgl(as, base|REX_64, jit_base);
  emit_addptr(as, base|REX_64, -1*delta*((int32_t)sizeof(TValue)));
  asm_guardcc(as, CC_NE);
  emit_cmp_bo_imm64(as, base, (int32_t)(-1 * ((int32_t)sizeof(TValue)) + offsetof(TValue, fr.tp)), ptr2imm64(pc));
}

/* -- Buffer operation ---------------------------------------------------- */

static void asm_bufhdr(ASMState *as, IRIns *ir)
{
  Reg sb = ra_dest(as, ir, RSET_GPR);

  if (ir->op2 == IRBUFHDR_APPEND) {
    /* Rematerialize const buffer pointer instead of likely spill. */
    IRIns *irp = IR(ir->op1);
    if (!(ra_hasreg(irp->r) || irp == ir - 1 ||
          (irp == ir - 2 && !ra_used(ir - 1)))) {
      while (!(irp->o == IR_BUFHDR && irp->op2 == IRBUFHDR_RESET))
        irp = IR(irp->op1);
      if (irref_isk(irp->op1)) {
        ra_weak(as, ra_allocref(as, ir->op1, RSET_GPR));
        ir = irp;
      }
    }
  } else {
    emit_movmroi(as, sb, offsetof(struct sbuf, sz), 0); /* sbuf->sz = 0; */
  }
  ra_left(as, sb, ir->op1);
}

static void asm_bufput(ASMState *as, IRIns *ir)
{
  const CCallInfo *ci = &lj_ir_callinfo[IRCALL_uj_sbuf_push_str];
  /* struct sbuf *sb, const GCstr *s */
  IRRef args[3] = {ir->op1, ir->op2, 0}; /* args[2] reserved for IR_SNEW fusion */
  size_t nargs = 2;
  IRIns *irs = IR(ir->op2);
  char kchar = '\0';

  lua_assert(irt_isstr(irs->t));
  if (irs->o == IR_KGC) {
    GCstr *s = ir_kstr(irs);

    if (s->len == 1) {  /* Optimize put of single-char string constant. */
      kchar = strdata(s)[0];
      args[1] = ASMREF_TMP1;  /* char */
      ci = &lj_ir_callinfo[IRCALL_uj_sbuf_push_char];
    }
  } else if (ra_noreg(irs->r)) {
    if (irs->o == IR_TOSTR) {  /* Fuse number to string conversions. */
      IRIns *irnum = IR(irs->op1);

      args[1] = irs->op1; /* lua_Number/int */

      if (irt_isnum(irnum->t))
        ci = &lj_ir_callinfo[IRCALL_uj_sbuf_push_number];
      else if (irt_isinteger(irnum->t))
        ci = &lj_ir_callinfo[IRCALL_uj_sbuf_push_int];
      else
        lua_assert(0);  /* Unreachable */
    } else if (irs->o == IR_SNEW) {
      nargs = 3;
      args[1] = irs->op1;  /* const void * */
      args[2] = irs->op2;  /* size_t */
      ci = &lj_ir_callinfo[IRCALL_uj_sbuf_push_block];
    }
    /* If 'irs' is now unused, it will be DCEd */
  }
  asm_setupresult(as, ir, ci);
  /*
   * NB: uj_sbuf_push_* interfaces can throw string buffer overflow error.
   * Any error thrown on a trace will lead to G->panic function call and exit.
   * This doesn't differ from any out-of-memory error raised during trace
   * execution. May be will be somehow fixed in future.
   */
  asm_gencall_nargs(as, ci, args, nargs);
  if (args[1] == ASMREF_TMP1)
    ra_allockreg(as, kchar, ra_releasetmp(as, args[1]));
}

static void asm_bufstr(ASMState *as, IRIns *ir)
{
  const CCallInfo *ci = &lj_ir_callinfo[IRCALL_uj_str_frombuf];
  /* lua_State *L, const struct uj_sbuf *sb */
  const IRRef args[2] = {ASMREF_L, ir->op1};
  as->gcsteps++;
  asm_setupresult(as, ir, ci);  /* GCstr * */
  asm_gencall(as, ci, args);
}

/* -- Type conversions ---------------------------------------------------- */

static void asm_tointg(ASMState *as, IRIns *ir, Reg left) {
  Reg tmp = ra_scratch(as, rset_exclude(RSET_FPR, left));
  Reg dest = ra_dest(as, ir, RSET_GPR);
  asm_guardcc(as, CC_P);
  asm_guardcc(as, CC_NE);
  emit_rr(as, XO_UCOMISD, left, tmp);
  emit_rr(as, XO_CVTSI2SD, tmp, dest);
  emit_rr(as, XO_XORPS, tmp, tmp);  /* Avoid partial register stall. */
  emit_rr(as, XO_CVTTSD2SI, dest, left);
  /* Can't fuse since left is needed twice. */
}

static void asm_tobit(ASMState *as, IRIns *ir) {
  Reg dest = ra_dest(as, ir, RSET_GPR);
  Reg tmp = ra_noreg(IR(ir->op1)->r) ?
              ra_alloc1(as, ir->op1, RSET_FPR) :
              ra_scratch(as, RSET_FPR);
  Reg right = asm_fuseload(as, ir->op2, rset_exclude(RSET_FPR, tmp));
  emit_rr(as, XO_MOVDto, tmp, dest);
  emit_mrm(as, XO_ADDSD, tmp, right);
  ra_left(as, tmp, ir->op1);
}

static void asm_conv(ASMState *as, IRIns *ir) {
  IRType st = (IRType)(ir->op2 & IRCONV_SRCMASK);
  int st64 = (st == IRT_I64 || st == IRT_U64 || st == IRT_P64);
  int stfp = (st == IRT_NUM || st == IRT_FLOAT);
  IRRef lref = ir->op1;
  lua_assert(irt_type(ir->t) != st);
  if (irt_isfp(ir->t)) {
    Reg dest = ra_dest(as, ir, RSET_FPR);
    if (stfp) {  /* FP to FP conversion. */
      Reg left = asm_fuseload(as, lref, RSET_FPR);
      emit_mrm(as, st == IRT_NUM ? XO_CVTSD2SS : XO_CVTSS2SD, dest, left);
      if (left == dest) { return;  /* Avoid the XO_XORPS. */ }
    } else {  /* Integer to FP conversion. */
      Reg left = (st == IRT_U32 || st == IRT_U64) ?
                 ra_alloc1(as, lref, RSET_GPR) :
                 asm_fuseloadm(as, lref, RSET_GPR, st64);
      if (st == IRT_U64) {
        const MCode *l_end = uj_emit_label(as);
        const void *k = lj_ir_k64_find(as->J, U64x(43f00000,00000000));
        emit_rma(as, XO_ADDSD, dest, k);  /* Add 2^64 to compensate. */
        uj_emit_jccs(as, CC_NS, l_end);
        emit_rr(as, XO_TEST, left|REX_64, left);  /* Check if u64 >= 2^63. */
      }
      emit_mrm(as, irt_isnum(ir->t) ? XO_CVTSI2SD : XO_CVTSI2SS,
               dest|((st64 || st == IRT_U32) ? REX_64 : 0), left);
    }
    emit_rr(as, XO_XORPS, dest, dest);  /* Avoid partial register stall. */
  } else if (stfp) {  /* FP to integer conversion. */
    if (irt_isguard(ir->t)) {
      /* Checked conversions are only supported from number to int. */
      lua_assert(irt_isint(ir->t) && st == IRT_NUM);
      asm_tointg(as, ir, ra_alloc1(as, lref, RSET_FPR));
    } else {
      Reg dest = ra_dest(as, ir, RSET_GPR);
      x86Op op = st == IRT_NUM ?
                 ((ir->op2 & IRCONV_TRUNC) ? XO_CVTTSD2SI : XO_CVTSD2SI) :
                 ((ir->op2 & IRCONV_TRUNC) ? XO_CVTTSS2SI : XO_CVTSS2SI);
      if (irt_isu64(ir->t)) {
        /* For inputs >= 2^63 add -2^64, convert again. */
        Reg tmp = ra_noreg(IR(lref)->r) ? ra_alloc1(as, lref, RSET_FPR) :
                                          ra_scratch(as, RSET_FPR);
        const MCode *l_end = uj_emit_label(as);
        emit_rr(as, op, dest|REX_64, tmp);
        if (st == IRT_NUM) {
          emit_rma(as, XO_ADDSD, tmp, lj_ir_k64_find(as->J, U64x(c3f00000,00000000)));
        } else {
          emit_rma(as, XO_ADDSS, tmp, lj_ir_k64_find(as->J, U64x(00000000,df800000)));
        }
        uj_emit_jccs(as, CC_NS, l_end);
        emit_rr(as, XO_TEST, dest|REX_64, dest);  /* Check if dest negative. */
        emit_rr(as, op, dest|REX_64, tmp);
        ra_left(as, tmp, lref);
      } else {
        Reg left = asm_fuseload(as, lref, RSET_FPR);
        if (irt_isu32(ir->t)) {
          emit_rr(as, XO_MOV, dest, dest);  /* Zero hiword. */
        }
        emit_mrm(as, op,
                 dest|((
                        irt_is64(ir->t) || irt_isu32(ir->t)) ? REX_64 : 0),
                 left);
      }
    }
  } else if (st >= IRT_I8 && st <= IRT_U16) {  /* Extend to 32 bit integer. */
    Reg left, dest = ra_dest(as, ir, RSET_GPR);
    RegSet allow = RSET_GPR;
    x86Op op;
    lua_assert(irt_isint(ir->t) || irt_isu32(ir->t));
    if (st == IRT_I8) {
      op = XO_MOVSXb; allow = RSET_GPR; dest |= FORCE_REX;
    } else if (st == IRT_U8) {
      op = XO_MOVZXb; allow = RSET_GPR; dest |= FORCE_REX;
    } else if (st == IRT_I16) {
      op = XO_MOVSXw;
    } else {
      op = XO_MOVZXw;
    }
    left = asm_fuseload(as, lref, allow);
    /* Add extra MOV if source is already in wrong register. */
    emit_mrm(as, op, dest, left);
  } else {  /* 32/64 bit integer conversions. */
    if (irt_is64(ir->t)) {
      Reg dest = ra_dest(as, ir, RSET_GPR);
      if (st64 || !(ir->op2 & IRCONV_SEXT)) {
        /* 64/64 bit no-op (cast) or 32 to 64 bit zero extension. */
        ra_left(as, dest, lref);  /* Do nothing, but may need to move regs. */
      } else {  /* 32 to 64 bit sign extension. */
        Reg left = asm_fuseload(as, lref, RSET_GPR);
        emit_mrm(as, XO_MOVSXd, dest|REX_64, left);
      }
    } else {
      Reg dest = ra_dest(as, ir, RSET_GPR);
      if (st64) {
        Reg left = asm_fuseload(as, lref, RSET_GPR);
        /* This is either a 32 bit reg/reg mov which zeroes the hiword
        ** or a load of the loword from a 64 bit address.
        */
        emit_mrm(as, XO_MOV, dest, left);
      } else {  /* 32/32 bit no-op (cast). */
        ra_left(as, dest, lref);  /* Do nothing, but may need to move regs. */
      }
    }
  }
}

static void asm_strto(ASMState *as, IRIns *ir) {
  /* Force a spill slot for the destination register (if any). */
  const CCallInfo *ci = &lj_ir_callinfo[IRCALL_uj_str_tonum];
  /* GCstr *str, TValue *n */
  const IRRef args[2] = {ir->op1, ASMREF_TMP1};
  RegSet drop = RSET_SCRATCH;
  ra_evictset(as, drop);
  asm_test_retval_zero(as, CC_E); /* side-exit if returned 0 */
  asm_gencall(as, ci, args);
  /* Store the result to the spill slot or temp slots. */
  emit_rmro(as, XO_LEA, ra_releasetmp(as, ASMREF_TMP1)|REX_64,
            RID_ESP, sps_scale(ir->s));
}

static void asm_tostr(ASMState *as, IRIns *ir) {
  const CCallInfo *ci;
  IRIns *irl = IR(ir->op1);
  /* lua_State *, lua_Number/int32_t */
  IRRef args[2] = {ASMREF_L, ir->op1};

  if (irt_isnum(irl->t))
    ci = &lj_ir_callinfo[IRCALL_uj_str_fromnumber];
  else
    ci = &lj_ir_callinfo[IRCALL_uj_str_fromint];

  as->gcsteps++;
  asm_setupresult(as, ir, ci); /* GCstr * */
  asm_gencall(as, ci, args);
}

/* -- Memory references --------------------------------------------------- */

static void asm_aref(ASMState *as, IRIns *ir) {
  IRRef irb = ir->op1;
  Reg dest;
  RegSet allow;
  if (ra_hasreg(IR(irb)->r)) {
    Reg base = IR(irb)->r;
    dest = ra_dest(as, ir, rset_exclude(RSET_GPR, base));
  } else {
    dest = ra_dest(as, ir, RSET_GPR);
  }
  allow = rset_exclude(RSET_GPR, dest);
  assert(IR(irb)->r != dest);

  asm_assert_abase(as, irb);
  Reg base = ra_alloc1(as, irb, allow);
  lua_assert(base != dest); /* Just to be double sure xD. */
  IRIns *irx = IR(ir->op2);
  if (irref_isk(ir->op2)) {
    int32_t ofs = sizeof(TValue)*irx->i;
    emit_rmro(as, XO_LEA, dest|REX_64, base, ofs);
  } else {
    Reg index;
    rset_clear(allow, base);
    index = ra_alloc1(as, ir->op2, allow);
    emit_rr(as, XO_ARITH(XOg_ADD), dest|REX_64, base);
    emit_shifti(as, XOg_SHL, dest, LOG_SIZEOF_TVALUE); /* shift to convert TValue* idx to byte offset. */
    emit_rr(as, XO_MOV, dest, index);
  }
}

/* Merge NE(HREF, niltv) check. */
static MCode *merge_href_niltv(ASMState *as, IRIns *ir) {
  /* Assumes nothing else generates NE of HREF. */
  if ((ir[1].o == IR_NE || ir[1].o == IR_EQ) && ir[1].op1 == as->curins &&
      ra_hasreg(ir->r)) {
    MCode *p = as->mcp;
    p += (*p != XI_ARITHi) ? 7+6 : 6+6;
    /* Ensure no loop branch inversion happened. */
    if (p[-6] == 0x0f && p[-5] == XI_JCCn+(CC_NE^(ir[1].o & 1))) {
      as->mcp = p;  /* Kill cmp reg, imm32 + jz exit. */
      return p + *(int32_t *)(p-4);  /* Return exit address. */
    }
  }
  return NULL;
}

/* Inlined hash lookup. Specialized for key type and for const keys.
** The equivalent C code is:
**   Node *n = hashkey(t, key);
**   do {
**     if (uj_obj_equal(&n->key, key)) return &n->val;
**   } while ((n = nextnode(n)));
**   return niltv(L);
*/
static void asm_href(ASMState *as, IRIns *ir) {
  MCode *nilexit = merge_href_niltv(as, ir);  /* Do this before any restores. */
  RegSet allow = RSET_GPR;
  Reg dest = ra_dest(as, ir, allow);
  Reg tab = ra_alloc1(as, ir->op1, rset_clear(allow, dest));
  Reg key = RID_NONE, tmp = RID_NONE;
  IRIns *irkey = IR(ir->op2);
  int isk = irref_isk(ir->op2);
  IRType1 kt = irkey->t;
  uint32_t khash;
  MCode *l_branch;
  const MCode *l_end;
  const MCode *l_next;

  if (!isk) {
    rset_clear(allow, tab);
    key = ra_alloc1(as, ir->op2, irt_isnum(kt) ? RSET_FPR : allow);
    if (!irt_isstr(kt)) {
      tmp = ra_scratch(as, rset_exclude(allow, key));
    }
  }

  /* Key not found in chain: jump to exit (if merged with NE) or load niltv. */
  l_end = uj_emit_label(as);
  if (nilexit && ir[1].o == IR_NE) {
    uj_emit_jccn(as, CC_E, nilexit);  /* XI_JMP is not found by lj_asm_patchexit. */
    nilexit = NULL;
  } else {
    emit_loada(as, dest, niltvg(J2G(as->J)));
  }

  /* Follow hash chain until the end. */
  l_branch = uj_emit_jccs_backedge(as, CC_NZ);
  emit_rr(as, XO_TEST, dest|REX_64, dest|REX_64);
  emit_rmro(as, XO_MOV, dest|REX_64, dest, offsetof(Node, next));
  l_next = uj_emit_label(as);

  /* Type and value comparison. */
  if (nilexit) {
    uj_emit_jccn(as, CC_E, nilexit);
  } else {
    uj_emit_jccs(as, CC_E, l_end);
  }

  if (irt_isnum(kt)) {
    if (isk) {
      /* Assumes -0.0 is already canonicalized to +0.0. */
      emit_cmp_bo_imm64(as, dest, offsetof(Node, key.u64), ir_knum(irkey)->u64);
    } else {
      uj_emit_jccs(as, CC_P, l_next);
      emit_rmro(as, XO_UCOMISD, key, dest, offsetof(Node, key.n));
    }
    uj_emit_jccs(as, CC_NE, l_next);
    /* The type check avoids NaN penalties and complaints from Valgrind. */
    emit_u32(as, LJ_TNUMX);
    emit_rmro(as, XO_ARITHi, XOg_CMP, dest, offsetof(Node, key.value_tag));
  } else if (irt_islightud(kt)) {
    emit_rmro(as, XO_CMP, key|REX_64, dest, offsetof(Node, key.u64));
  } else {
    if (!irt_ispri(kt)) {
      lua_assert(irt_isaddr(kt));
      if (isk) {
        emit_cmp_bo_imm64(as, dest, offsetof(Node, key.gcr), ptr2imm64(ir_kgc(irkey)));
      } else {
        emit_rmro(as, XO_CMP, key|REX_64, dest, offsetof(Node, key.gcr));
      }
      uj_emit_jccs(as, CC_NE, l_next);
    }
    lua_assert(!irt_isnil(kt));
    emit_i8(as, irt_toitype(kt));
    emit_rmro(as, XO_ARITHi8, XOg_CMP, dest, offsetof(Node, key.value_tag));
  }
  uj_emit_jcc_fixup(l_branch, as->mcp);
  checkmclim(as);

  /* Load main position relative to tab->node into dest. */
  khash = isk ? ir_khash(irkey) : 1;
  if (khash == 0) {
    emit_rmro(as, XO_MOV, dest|REX_64, tab, offsetof(GCtab, node));
  } else {
    /* GCtab.node points to wide address space (64-bit reg), so overall
    ** node pointer value should be stored in 64-bit reg too.
    */
    emit_rmro(as, XO_ARITH(XOg_ADD), dest|REX_64, tab, offsetof(GCtab, node));

    /* IMUL by sizeof(Node) below is used to calculate offset in Node* array based
    ** on the node index in array. Initially, sizeof(Node) was 24 bytes, thus the
    ** the preferred solution was LEA with scaled base index + shift:
    **
    ** emit_shifti(as, XOg_SHL, dest, 3);
    ** emit_rmrxo(as, XO_LEA, dest, dest, dest, XM_SCALE2, 0);
    **
    ** As of Nov 2016, sizeof(Node) is 40 bytes, but
    **
    **  lea r, [r + 4 * r]
    **  shl r, 3
    **
    ** does not work significantly faster than
    **
    **  imul r, r, 40
    **
    ** So we stick with a more conservative solution with IMUL for now.
    */
    emit_i8(as, sizeof(Node));
    emit_rr(as, XO_IMULi8, dest, dest);

    if (isk) {
      emit_gri(as, XG_ARITHi(XOg_AND), dest, (int32_t)khash);
      emit_rmro(as, XO_MOV, dest, tab, offsetof(GCtab, hmask));
    } else if (irt_isstr(kt)) {
      emit_rmro(as, XO_ARITH(XOg_AND), dest, key, offsetof(GCstr, hash));
      emit_rmro(as, XO_MOV, dest, tab, offsetof(GCtab, hmask));
    } else {  /* Must match with hashrot() in lj_tab.c. */
      emit_rmro(as, XO_ARITH(XOg_AND), dest, tab, offsetof(GCtab, hmask));
      emit_rr(as, XO_ARITH(XOg_SUB), dest, tmp);
      emit_shifti(as, XOg_ROL, tmp, HASH_ROT3);
      emit_rr(as, XO_ARITH(XOg_XOR), dest, tmp);
      emit_shifti(as, XOg_ROL, dest, HASH_ROT2);
      emit_rr(as, XO_ARITH(XOg_SUB), tmp, dest);
      emit_shifti(as, XOg_ROL, dest, HASH_ROT1);
      emit_rr(as, XO_ARITH(XOg_XOR), tmp, dest);
      if (irt_isnum(kt)) {
        emit_rr(as, XO_ARITH(XOg_ADD), dest, dest);
        emit_shifti(as, XOg_SHR|REX_64, dest, 32);
        emit_rr(as, XO_MOV, tmp, dest);
        emit_rr(as, XO_MOVDto, key|REX_64, dest);
      } else {
        emit_rr(as, XO_MOV, tmp, key);
        emit_rmro(as, XO_LEA, dest, key, HASH_BIAS);
      }
    }
  }
}

static void asm_hrefk(ASMState *as, IRIns *ir) {
  IRIns *kslot = IR(ir->op2);
  IRIns *irkey = IR(kslot->op1);
  int32_t ofs = (int32_t)(kslot->op2 * sizeof(Node));
  Reg dest = ra_used(ir) ? ra_dest(as, ir, RSET_GPR) : RID_NONE;
  Reg node = ra_alloc1(as, ir->op1, RSET_GPR);
  lua_assert(ofs % sizeof(Node) == 0);
  if (ra_hasreg(dest)) {
    if (ofs != 0) {
      if (dest == node) {
        emit_gri(as, XG_ARITHi(XOg_ADD), dest|REX_64, ofs);
      } else {
        emit_rmro(as, XO_LEA, dest|REX_64, node, ofs);
      }
    } else if (dest != node) {
      emit_rr(as, XO_MOV, dest|REX_64, node);
    }
  }

  /* Check value tag. */
  lua_assert(!irt_isnil(irkey->t));
  asm_guardcc(as, CC_NE);
  emit_i8(as, irt_toitype(irkey->t));
  emit_rmro(as, XO_ARITHi8, XOg_CMP, node,
    ofs + (int32_t)offsetof(Node, key.value_tag));

  /* For non-primitive types, also check the payload. */
  if (!irt_ispri(irkey->t)) {
    Reg key = ra_scratch(as, rset_exclude(RSET_GPR, node));
    asm_guardcc(as, CC_NE);
    emit_rmro(as, XO_CMP, key|REX_64, node,
      ofs + (int32_t)offsetof(Node, key.u64));
    /* Assumes -0.0 is already canonicalized to +0.0. */
    if (irt_isnum(irkey->t)) {
      emit_loadu64(as, key, rawV(ir_knum(irkey)));
    } else {
      emit_loada(as, key, ir_kgc(irkey));
    }
  }
}

static void asm_newref(ASMState *as, IRIns *ir) {
  const CCallInfo *ci = &lj_ir_callinfo[IRCALL_lj_tab_newkey];
  /* lua_State *L, GCtab *t, const TValue *key */
  const IRRef args[3] = {ASMREF_L, ir->op1, ASMREF_TMP1};
  IRIns *irkey;
  Reg tmptv_p;
  if (ir->r == RID_SINK) { return; }
  asm_setupresult(as, ir, ci);  /* TValue * */
  asm_gencall(as, ci, args);
  tmptv_p = ra_releasetmp(as, ASMREF_TMP1);
  irkey = IR(ir->op2);

  /* Emit the key pointer for lj_tab_newkey call.
  ** In case the key is number constant, pointer to constant TValue is used.
  ** In other cases, &g->tmptv is properly filled and passed.
  */
  if (irt_isnum(irkey->t) && irref_isk(ir->op2)) {
    emit_loada(as, tmptv_p, ir_knum(irkey));
  } else {
    /* Store the payload to tmptv. */
    if (irt_isnum(irkey->t) /* && !irref_isk(ir->op2) */) {
      Reg src = ra_alloc1(as, ir->op2, RSET_FPR);
      emit_rmro(as, XO_MOVSDto, src, tmptv_p, 0);
    } else {
      lua_assert(irt_ispri(irkey->t) || irt_isaddr(irkey->t));
      if (!irref_isk(ir->op2)) {
        Reg src = ra_alloc1(as, ir->op2, rset_exclude(RSET_GPR, tmptv_p));
        emit_movtomro(as, REX_64IR(irkey, src), tmptv_p, 0);
        if (!(REX_64IR(irkey, src) & REX_64)) { emit_movmroi(as, tmptv_p, 4, 0); }
      } else {
        emit_movmroi64(as, tmptv_p, 0, irkey->i);
      }
    }

    /* Store key tag to tmptv. */
    emit_movmroi(as, tmptv_p, offsetof(TValue, value_tag), irt_toitype(irkey->t));

    /* Get &g->tmptv. */
    emit_loada(as, tmptv_p, &J2G(as->J)->tmptv);
  }
}

static void asm_lref(ASMState *as, IRIns *ir) {
  Reg r = ra_dest(as, ir, RSET_GPR);
  ra_left(as, r, ASMREF_L);
}

static void asm_uref(ASMState *as, IRIns *ir) {
  Reg dest = ra_dest(as, ir, RSET_GPR);
  if (irref_isk(ir->op1)) {
    GCfunc *fn = ir_kfunc(IR(ir->op1));
    TValue **v = &(fn->l.uvptr[(ir->op2 >> 8)])->v;
    emit_rma(as, XO_MOV, dest|REX_64, v);
  } else {
    Reg uv = ra_scratch(as, RSET_GPR);
    Reg func = ra_alloc1(as, ir->op1, RSET_GPR);
    if (ir->o == IR_UREFC) {
      emit_rmro(as, XO_LEA, dest|REX_64, uv, offsetof(GCupval, tv));
      asm_guardcc(as, CC_NE);
      emit_i8(as, 1);
      emit_rmro(as, XO_ARITHib, XOg_CMP, uv, offsetof(GCupval, closed));
    } else {
      emit_rmro(as, XO_MOV, dest|REX_64, uv, offsetof(GCupval, v));
    }
    emit_rmro(as, XO_MOV, uv|REX_64, func,
              (int32_t)offsetof(GCfuncL, uvptr) + 8*(int32_t)(ir->op2 >> 8));
  }
}

static void asm_fref(ASMState *as, IRIns *ir) {
  Reg dest = ra_dest(as, ir, RSET_GPR);
  uint8_t ofs = field_ofs[ir->op2];
  if(irref_isk(ir->op1)) {
    emit_loada(as, dest, ir_kgc(IR(ir->op1)) + ofs);
  } else {
    Reg src = ra_alloc1(as, ir->op1, RSET_GPR);
    emit_rmro(as, XO_LEA, dest|REX_64, src, ofs);
  }
}

static void asm_strref(ASMState *as, IRIns *ir) {
  Reg dest = ra_dest(as, ir, RSET_GPR);
  asm_fusestrref(as, ir, RSET_GPR);
  if (as->mrm.base == RID_NONE) {
    /* Wrong, but not going to happen due to fusestrref fix. */
    emit_loadi(as, dest, as->mrm.ofs);
  } else if (as->mrm.base == dest && as->mrm.idx == RID_NONE) {
    emit_gri(as, XG_ARITHi(XOg_ADD), dest|REX_64, as->mrm.ofs);
  } else {
    emit_mrm(as, XO_LEA, dest|REX_64, RID_MRM);
  }
}

/* -- Loads and stores ---------------------------------------------------- */

static void asm_fxload(ASMState *as, IRIns *ir) {
  Reg dest = ra_dest(as, ir, irt_isfp(ir->t) ? RSET_FPR : RSET_GPR);
  x86Op xo;
  if (irt_istab(ir->t) && asm_needs_movtv(as, ir)) {
    asm_guard_nomt(as, ir, dest);
  }
  if (ir->o == IR_FLOAD) {
    asm_fusefref(as, ir, RSET_GPR);
  } else {
    asm_fusexref(as, ir->op1, RSET_GPR);
  }
  /* ir->op2 is ignored -- unaligned loads are ok on x86. */
  switch (irt_type(ir->t)) {
  case IRT_I8: xo = XO_MOVSXb; break;
  case IRT_U8: xo = XO_MOVZXb; break;
  case IRT_I16: xo = XO_MOVSXw; break;
  case IRT_U16: xo = XO_MOVZXw; break;
  case IRT_NUM: xo = XO_MOVSD; break;
  case IRT_FLOAT: xo = XO_MOVSS; break;
  default:
    if (irt_is64(ir->t)) {
      dest |= REX_64;
    } else {
      lua_assert(irt_isint(ir->t) || irt_isu32(ir->t) || irt_isaddr(ir->t));
    }
    xo = XO_MOV;
    break;
  }
  emit_mrm(as, xo, dest, RID_MRM);
}

static void asm_fxstore(ASMState *as, IRIns *ir) {
  RegSet allow = RSET_GPR;
  Reg src = RID_NONE;
  int32_t k = 0;
  if (ir->r == RID_SINK) { return; }
  /* The IRT_I16/IRT_U16 stores should never be simplified for constant
  ** values since mov word [mem], imm16 has a length-changing prefix.
  */
  if (irt_isi16(ir->t) || irt_isu16(ir->t) || irt_isfp(ir->t) ||
      !asm_isk32(as, ir->op2, &k)) {
    RegSet allow8 = irt_isfp(ir->t) ? RSET_FPR : RSET_GPR;
    src = ra_alloc1(as, ir->op2, allow8);
    rset_clear(allow, src);
  }
  if (ir->o == IR_FSTORE) {
    asm_fusefref(as, IR(ir->op1), allow);
  } else {
    asm_fusexref(as, ir->op1, allow);
  }
  if (ra_hasreg(src)) {
    x86Op xo;
    switch (irt_type(ir->t)) {
    case IRT_I8: case IRT_U8: xo = XO_MOVtob; src |= FORCE_REX; break;
    case IRT_I16: case IRT_U16: xo = XO_MOVtow; break;
    case IRT_NUM: xo = XO_MOVSDto; break;
    case IRT_FLOAT: xo = XO_MOVSSto; break;
    case IRT_LIGHTUD: lua_assert(0);  /* NYI: mask 64 bit lightuserdata. */
    default:
      if (irt_is64(ir->t)) {
        src |= REX_64;
      } else {
        lua_assert(irt_isint(ir->t) || irt_isu32(ir->t) || irt_isaddr(ir->t));
      }
      xo = XO_MOVto;
      break;
    }
    emit_mrm(as, xo, src, RID_MRM);
  } else {
    if (irt_isi8(ir->t) || irt_isu8(ir->t)) {
      emit_i8(as, k);
      emit_mrm(as, XO_MOVmib, 0, RID_MRM);
    } else {
      lua_assert(irt_is64(ir->t) || irt_isint(ir->t) || irt_isu32(ir->t) ||
                 irt_isaddr(ir->t));
      emit_i32(as, k);
      emit_mrm(as, XO_MOVmi, REX_64IR(ir, 0), RID_MRM);
    }
  }
}

static Reg asm_load_lightud64(ASMState *as, IRIns *ir, int typecheck) {
  if (ra_used(ir) || typecheck) {
    Reg dest = ra_dest(as, ir, RSET_GPR);
    if (typecheck) {
      Reg tmp = ra_scratch(as, rset_exclude(RSET_GPR, dest));
      asm_guardcc(as, CC_NE);
      emit_i8(as, -2);
      emit_rr(as, XO_ARITHi8, XOg_CMP, tmp);
      emit_shifti(as, XOg_SAR|REX_64, tmp, 47);
      emit_rr(as, XO_MOV, tmp|REX_64, dest);
    }
    return dest;
  } else {
    return RID_NONE;
  }
}

static void asm_ahuvload(ASMState *as, IRIns *ir) {
  lua_assert(irt_isnum(ir->t) ||
             irt_ispri(ir->t) ||
             irt_isaddr(ir->t) ||
             irt_istval(ir->t));

  if (irt_istval(ir->t)) {
    Reg dest = ra_dest(as, ir, RSET_FPR);
    Reg ref = ra_alloc1(as, ir->op1, RSET_GPR);

    lua_assert(ir_hashint(ir, IRH_MOVTV));

    emit_mxmmm128(as, dest, ref);
    return;
  }

  if (irt_islightud(ir->t)) {
    Reg dest = asm_load_lightud64(as, ir, 1);
    if (ra_hasreg(dest)) {
      asm_fuseahuref(as, ir, RSET_GPR);
      emit_mrm(as, XO_MOV, dest|REX_64, RID_MRM);
    }
    return;
  } else if (ra_used(ir)) {
    RegSet allow = irt_isnum(ir->t) ? RSET_FPR : RSET_GPR;
    Reg dest = ra_dest(as, ir, allow);
    if (irt_istab(ir->t) && asm_needs_movtv(as, ir)) {
      asm_guard_nomt(as, ir, dest);
    }
    asm_fuseahuref(as, ir, RSET_GPR);
    emit_mrm(as, dest < RID_MAX_GPR ? XO_MOV : XO_MOVSD, REX_64IR(ir, dest), RID_MRM);
  } else {
    asm_fuseahuref(as, ir, RSET_GPR);
  }
  /* Always do the type check, even if the load result is unused. */
  as->mrm.ofs += offsetof(TValue, value_tag);

  asm_guardcc(as, CC_NE);
  if (irt_type(ir->t) >= IRT_NUM) {
    lua_assert(irt_isinteger(ir->t) || irt_isnum(ir->t));
    emit_u32(as, LJ_TNUMX);
    emit_mrm(as, XO_ARITHi, XOg_CMP, RID_MRM);
  } else {
    emit_i8(as, irt_toitype(ir->t));
    emit_mrm(as, XO_ARITHi8, XOg_CMP, RID_MRM);
  }
}

static void asm_ahustore(ASMState *as, IRIns *ir) {
  if (ir->r == RID_SINK) { return; }

  if (irt_istval(ir->t)) {
    lua_assert(ir_hashint(ir, IRH_MOVTV));

    if (irt_istval(IR(ir->op2)->t)) {
      Reg dst = ra_alloc1(as, ir->op1, RSET_GPR);
      Reg src = ra_alloc1(as, ir->op2, RSET_FPR);

      emit_mm128xmm(as, dst, src);
    } else {
      Reg dst, src;
      Reg tmp = ra_scratch(as, RSET_FPR);
      RegSet allow = RSET_GPR;

      dst = ra_alloc1(as, ir->op1, allow);
      allow = rset_exclude(allow, dst);
      src = ra_alloc1(as, ir->op2, allow);

      /* The same as movtv (VM core) or copyTV (runtime): */
      emit_mm128xmm(as, dst, tmp);
      emit_mxmmm128(as, tmp, src);
    }

    return;
  }

  if (irt_isnum(ir->t)) {
    Reg src = ra_alloc1(as, ir->op2, RSET_FPR);
    asm_fuseahuref(as, ir, RSET_GPR);
    emit_mrm(as, XO_MOVSDto, src, RID_MRM);
  } else if (irt_islightud(ir->t)) {
    Reg src = ra_alloc1(as, ir->op2, RSET_GPR);
    asm_fuseahuref(as, ir, rset_exclude(RSET_GPR, src));
    emit_mrm(as, XO_MOVto, src|REX_64, RID_MRM);
  } else {
    IRIns *irr = IR(ir->op2);
    RegSet allow = RSET_GPR;
    Reg src = RID_NONE;
    if (!irref_isk(ir->op2)) {
      src = ra_alloc1(as, ir->op2, allow);
      rset_clear(allow, src);
    }
    asm_fuseahuref(as, ir, allow);
    if (ra_hasreg(src)) {
      emit_mrm(as, XO_MOVto, src|REX_64, RID_MRM);
    } else if (!irt_ispri(irr->t)) {
      //lua_assert(0); /* need to store 64-bit constant to base+ofs */
      lua_assert(irt_isaddr(ir->t));
      //emit_i32(as, irr->i);
      //emit_mrm(as, XO_MOVmi, 0, RID_MRM);

      emit_mrm(as, XO_MOVto, RID_INTERNAL|REX_64, RID_MRM);
      emit_loadu64_(as, RID_INTERNAL, irr->i);
    }

    /* !! Temporary workaround for scratching hi dword of dst. */
    //as->mrm.ofs += 4;
    //emit_i32(as, 0);
    //emit_mrm(as, XO_MOVmi, 0, RID_MRM);
  }

  //if (irt_isnum(ir->t) || irt_islightud(ir->t)) {
    as->mrm.ofs += 8; /* as->mrm.ofs was pointing at TValue. */
  //} else {
  //  as->mrm.ofs += 4; /* as->mrm.ofs was pointing at TValue.u32.hi */
  //}
  emit_i32(as, (int32_t)irt_toitype(ir->t));
  emit_mrm(as, XO_MOVmi, 0, RID_MRM);
}

static void asm_sload(ASMState *as, IRIns *ir) {
  int32_t offset = ir_sload_offset(ir);
  IRType1 t = ir->t;
  Reg base;
  lua_assert(!(ir->op2 & IRSLOAD_PARENT));  /* Handled by asm_head_side(). */
  lua_assert(irt_isguard(t) || !ir_sload_typecheck(ir));
  lua_assert(!irt_isint(t) || (ir->op2 & (IRSLOAD_CONVERT|IRSLOAD_FRAME)));
  if ((ir->op2 & IRSLOAD_CONVERT) && irt_isguard(t) && irt_isint(t)) {
    Reg left = ra_scratch(as, RSET_FPR);
    asm_tointg(as, ir, left);  /* Frees dest reg. Do this before base alloc. */
    base = ra_alloc1(as, REF_BASE, RSET_GPR);
    emit_rmro(as, XO_MOVSD, left, base, offset);
    t.irt = IRT_NUM;  /* Continue with a regular number type check. */
  } else if (irt_islightud(t)) {
    Reg dest = asm_load_lightud64(as, ir, ir_sload_typecheck(ir));
    if (ra_hasreg(dest)) {
      base = ra_alloc1(as, REF_BASE, RSET_GPR);
      emit_rmro(as, XO_MOV, dest|REX_64, base, offset);
    }
    return;
  } else if (ra_used(ir)) {
    RegSet allow = irt_isnum(t) ? RSET_FPR : RSET_GPR;
    Reg dest = ra_dest(as, ir, allow);
    base = ra_alloc1(as, REF_BASE, RSET_GPR);
    lua_assert(irt_isnum(t) || irt_isint(t) || irt_isaddr(t) || irt_isu32(t));
    if (irt_istab(ir->t) && asm_needs_movtv(as, ir)) {
      asm_guard_nomt(as, ir, dest);
    }
    if ((ir->op2 & IRSLOAD_CONVERT)) {
      t.irt = irt_isint(t) ? IRT_NUM : IRT_INT;  /* Check for original type. */
      emit_rmro(as, irt_isint(t) ? XO_CVTSI2SD : XO_CVTSD2SI, dest, base, offset);
    } else if (irt_isnum(t)) {
      emit_rmro(as, XO_MOVSD, dest, base, offset);
    } else {
      const uint32_t REX = irt_is64(t) ? REX_64 : 0;
      emit_rmro(as, XO_MOV, dest | REX, base, offset);
    }
  } else {
    if (!ir_sload_typecheck(ir)) {
      return;  /* No type check: avoid base alloc. */
    }
    base = ra_alloc1(as, REF_BASE, RSET_GPR);
  }
  if (ir_sload_typecheck(ir)) {
    /* Need type check, even if the load result is unused. */
    asm_guardcc(as, CC_NE);
    if (irt_type(t) >= IRT_NUM) {
      lua_assert(irt_isinteger(t) || irt_isnum(t) || irt_isu32(t));
      emit_u32(as, LJ_TNUMX);
      emit_rmro(as, XO_ARITHi, XOg_CMP, base, offset + offsetof(TValue, value_tag));
    } else {
      emit_i8(as, irt_toitype(t));
      emit_rmro(as, XO_ARITHi8, XOg_CMP, base, offset + offsetof(TValue, value_tag));
    }
  }
}

/* -- Allocations --------------------------------------------------------- */

#if LJ_HASFFI
static void asm_cnew(ASMState *as, IRIns *ir) {
  CTState *cts = ctype_ctsG(J2G(as->J));
  CTypeID ctypeid = (CTypeID)IR(ir->op1)->i;
  CTSize sz = (ir->o == IR_CNEWI || ir->op2 == REF_NIL) ?
              lj_ctype_size(cts, ctypeid) : (CTSize)IR(ir->op2)->i;
  const CCallInfo *ci = &lj_ir_callinfo[IRCALL_uj_obj_new];
  /* lua_State *L, size_t size */
  const IRRef args[2] = {ASMREF_L, ASMREF_TMP1};
  lua_assert(sz != CTSIZE_INVALID);

  as->gcsteps++;
  asm_setupresult(as, ir, ci);  /* GCcdata * */

  /* Initialize immutable cdata object. */
  if (ir->o == IR_CNEWI) {
    RegSet allow = (RSET_GPR & ~RSET_SCRATCH);
    Reg r64 = sz == 8 ? REX_64 : 0;
    if (irref_isk(ir->op2)) {
      IRIns *irk = IR(ir->op2);
      uint64_t k = irk->o == IR_KINT64 ? rawV(ir_k64(irk)) : irk->i;
      if (sz == 4 || checki32((int64_t)k)) {
        emit_i32(as, (int32_t)k);
        emit_rmro(as, XO_MOVmi, r64, RID_RET, sizeof(GCcdata));
      } else {
        emit_movtomro(as, RID_ECX + r64, RID_RET, sizeof(GCcdata));
        emit_loadu64(as, RID_ECX, k);
      }
    } else {
      Reg r = ra_alloc1(as, ir->op2, allow);
      emit_movtomro(as, r + r64, RID_RET, sizeof(GCcdata));
    }
    lua_assert(sz == 4 || sz == 8);
  }

  /* Combine initialization of marked, gct and ctypeid. */
  emit_movtomro(as, RID_ECX, RID_RET, offsetof(GCcdata, marked));
  emit_gri(as, XG_ARITHi(XOg_OR), RID_ECX,
           (int32_t)((~LJ_TCDATA<<8)+(ctypeid<<16)));
  emit_gri(as, XG_ARITHi(XOg_AND), RID_ECX, LJ_GC_WHITES);
  emit_opgl(as, XO_MOVZXb, RID_ECX, gc.currentwhite);

  asm_gencall(as, ci, args);
  emit_loadi(as, ra_releasetmp(as, ASMREF_TMP1), (int32_t)(sz+sizeof(GCcdata)));
}
#else
#define asm_cnew(as, ir)        ((void)0)
#endif

/* -- Write barriers ------------------------------------------------------ */

static void asm_tbar(ASMState *as, IRIns *ir) {
  Reg tab = ra_alloc1(as, ir->op1, RSET_GPR);
  Reg tmp = ra_scratch(as, rset_exclude(RSET_GPR, tab));
  const MCode *l_end = uj_emit_label(as);
  emit_movtomro(as, tmp|REX_64, tab, offsetof(GCtab, gclist));
  emit_setgl(as, tab|REX_64, gc.grayagain);
  emit_getgl(as, tmp|REX_64, gc.grayagain);
  emit_i8(as, ~LJ_GC_BLACK);
  emit_rmro(as, XO_ARITHib, XOg_AND, tab, offsetof(GCtab, marked));
  uj_emit_jccs(as, CC_Z, l_end);
  emit_i8(as, LJ_GC_BLACK);
  emit_rmro(as, XO_GROUP3b, XOg_TEST, tab, offsetof(GCtab, marked));
}

static void asm_obar(ASMState *as, IRIns *ir) {
  const CCallInfo *ci = &lj_ir_callinfo[IRCALL_lj_gc_barrieruv];
  /* global_State *g, TValue *tv */
  const IRRef args[2] = {ASMREF_TMP1, ir->op1};
  const MCode *l_end;
  Reg obj;
  /* No need for other object barriers (yet). */
  lua_assert(IR(ir->op1)->o == IR_UREFC);
  ra_evictset(as, RSET_SCRATCH);
  l_end = uj_emit_label(as);
  asm_gencall(as, ci, args);
  emit_loada(as, ra_releasetmp(as, ASMREF_TMP1), J2G(as->J));
  obj = IR(ir->op1)->r;
  uj_emit_jccs(as, CC_Z, l_end);
  emit_i8(as, LJ_GC_WHITES);
  if (irref_isk(ir->op2)) {
    GCobj *vp = ir_kgc(IR(ir->op2));
    emit_rma(as, XO_GROUP3b, XOg_TEST, &vp->gch.marked);
  } else {
    Reg val = ra_alloc1(as, ir->op2, rset_exclude(RSET_SCRATCH&RSET_GPR, obj));
    emit_rmro(as, XO_GROUP3b, XOg_TEST, val, (int32_t)offsetof(GChead, marked));
  }
  uj_emit_jccs(as, CC_Z, l_end);
  emit_i8(as, LJ_GC_BLACK);
  emit_rmro(as, XO_GROUP3b, XOg_TEST, obj,
            (int32_t)offsetof(GCupval, marked)-(int32_t)offsetof(GCupval, tv));
}

/* -- FP/int arithmetic and logic operations ------------------------------ */

/* Load reference onto x87 stack. Force a spill to memory if needed. */
static void asm_x87load(ASMState *as, IRRef ref) {
  IRIns *ir = IR(ref);
  if (ir->o == IR_KNUM) {
    const TValue *tv = ir_knum(ir);
    if (tvispzero(tv)) { /* Use fldz only for +0. */
      emit_x87op(as, XI_FLDZ);
    } else if (tvispone(tv)) {
      emit_x87op(as, XI_FLD1);
    } else {
      emit_rma(as, XO_FLDq, XOg_FLDq, tv);
    }
  } else {
    emit_mrm(as, XO_FLDq, XOg_FLDq, asm_fuseload(as, ref, RSET_EMPTY));
  }
}

/* Emit call to en external function which return double */
static void asm_external_fpmath(ASMState *as, IRIns *ir, const void *function) {
  /* External call may change all but callee saved registers */
  RegSet drop = RSET_SCRATCH;
  if (ra_hasreg(ir->r)) {
    rset_clear(drop, ir->r);  /* Dest reg handled below. */
  }
  ra_evictset(as, drop);
  ra_destreg(as, ir, RID_XMM0);
  uj_emit_call(as, function);
}

/* Try to rejoin pow from EXP2, MUL and LOG2 (if still unsplit). */
static int fpmjoin_pow(ASMState *as, IRIns *ir) {
  IRIns *irp = IR(ir->op1);
  if (irp == ir-1 && irp->o == IR_MUL && !ra_used(irp)) {
    IRIns *irpp = IR(irp->op1);
    if (irpp == ir-2 && irpp->o == IR_FPMATH &&
        irpp->op2 == IRFPM_LOG2 && !ra_used(irpp)) {
UJ_PEDANTIC_OFF /* casting a function ptr to void* */
      asm_external_fpmath(as, ir, pow);
UJ_PEDANTIC_ON
      IRIns *irx;
      irx = IR(irpp->op1);
      if (ra_noreg(irx->r) && ra_gethint(irx->r) == RID_XMM1) {
        irx->r = RID_INIT;  /* Avoid allocating xmm1 for x. */
      }
      ra_left(as, RID_XMM0, irpp->op1);
      ra_left(as, RID_XMM1, irp->op2);
      return 1;
    }
  }
  return 0;
}

static void asm_fpmath(ASMState *as, IRIns *ir) {
  IRFPMathOp fpm = ir->o == IR_FPMATH ? (IRFPMathOp)ir->op2 : IRFPM_OTHER;
  if (fpm == IRFPM_SQRT) {
    Reg dest = ra_dest(as, ir, RSET_FPR);
    Reg left = asm_fuseload(as, ir->op1, RSET_FPR);
    emit_mrm(as, XO_SQRTSD, dest, left);
  } else if (fpm <= IRFPM_TRUNC) {
    if (as->flags & JIT_F_SSE4_1) {  /* SSE4.1 has a rounding instruction. */
      Reg dest = ra_dest(as, ir, RSET_FPR);
      Reg left = asm_fuseload(as, ir->op1, RSET_FPR);
      /* ROUNDSD has a 4-byte opcode which doesn't fit in x86Op.
      ** Let's pretend it's a 3-byte opcode, and compensate afterwards.
      ** This is atrocious, but the alternatives are much worse.
      */
      /* Round down/up/trunc == 1001/1010/1011. */
      emit_i8(as, 0x09 + fpm);
      emit_mrm(as, XO_ROUNDSD, dest, left);
      if (as->mcp[1] != (MCode)(XO_ROUNDSD >> 16)) {
        as->mcp[0] = as->mcp[1]; as->mcp[1] = 0x0f;  /* Swap 0F and REX. */
      }
      *--as->mcp = 0x66;  /* 1st byte of ROUNDSD opcode. */
    } else {  /* Call helper functions for SSE2 variant. */
UJ_PEDANTIC_OFF /* casting a function ptr to void* */
      asm_external_fpmath(as, ir, fpm == IRFPM_FLOOR ? floor :
                           fpm == IRFPM_CEIL ? ceil : trunc);
UJ_PEDANTIC_ON
      ra_left(as, RID_XMM0, ir->op1);
    }
  } else if (fpm == IRFPM_EXP2 && fpmjoin_pow(as, ir)) {
    /* Rejoined to pow(). */
  } else if (fpm == IRFPM_EXP || fpm == IRFPM_EXP2) {
UJ_PEDANTIC_OFF /* casting a function ptr to void* */
    asm_external_fpmath(as, ir, fpm == IRFPM_EXP ? exp : exp2);
UJ_PEDANTIC_ON
    ra_left(as, RID_XMM0, ir->op1);
  } else {  /* Handle x87 ops. */
    int32_t ofs = sps_scale(ir->s);  /* Use spill slot or temp slots. */
    Reg dest = ir->r;
    if (ra_hasreg(dest)) {
      ra_free(as, dest);
      ra_modified(as, dest);
      emit_rmro(as, XO_MOVSD, dest, RID_ESP, ofs);
    }
    emit_rmro(as, XO_FSTPq, XOg_FSTPq, RID_ESP, ofs);
    switch (fpm) {  /* st0 = lj_vm_*(st0) */
    case IRFPM_SIN: emit_x87op(as, XI_FSIN); break;
    case IRFPM_COS: emit_x87op(as, XI_FCOS); break;
    case IRFPM_TAN: emit_x87op(as, XI_FPOP); emit_x87op(as, XI_FPTAN); break;
    case IRFPM_LOG: case IRFPM_LOG2: case IRFPM_LOG10:
      /* Note: the use of fyl2xp1 would be pointless here. When computing
      ** log(1.0+eps) the precision is already lost after 1.0 is added.
      ** Subtracting 1.0 won't recover it. OTOH math.log1p would make sense.
      */
      emit_x87op(as, XI_FYL2X); break;
    case IRFPM_OTHER:
      switch (ir->o) {
      case IR_ATAN2:
        emit_x87op(as, XI_FPATAN); asm_x87load(as, ir->op2); break;
      case IR_LDEXP:
        emit_x87op(as, XI_FPOP1); emit_x87op(as, XI_FSCALE); break;
      default: lua_assert(0); break;
      }
      break;
    default: lua_assert(0); break;
    }
    asm_x87load(as, ir->op1);
    switch (fpm) {
    case IRFPM_LOG: emit_x87op(as, XI_FLDLN2); break;
    case IRFPM_LOG2: emit_x87op(as, XI_FLD1); break;
    case IRFPM_LOG10: emit_x87op(as, XI_FLDLG2); break;
    case IRFPM_OTHER:
      if (ir->o == IR_LDEXP) asm_x87load(as, ir->op2);
      break;
    default: break;
    }
  }
}

static void asm_fppowi(ASMState *as, IRIns *ir) {
UJ_PEDANTIC_OFF /* casting a function ptr to void* */
  asm_external_fpmath(as, ir, uj_math_powi);
UJ_PEDANTIC_ON
  ra_left(as, RID_XMM0, ir->op1);
  ra_left(as, RID_EDI, ir->op2);
}

#if LJ_HASFFI
static void asm_arith64(ASMState *as, IRIns *ir, IRCallID id) {
  const CCallInfo *ci = &lj_ir_callinfo[id];
  const IRRef args[2] = {ir->op1, ir->op2};
  asm_setupresult(as, ir, ci);
  asm_gencall(as, ci, args);
}
#endif

static void asm_intmod(ASMState *as, IRIns *ir) {
  const CCallInfo *ci = &lj_ir_callinfo[IRCALL_uj_math_modi];
  const IRRef args[2] = {ir->op1, ir->op2};
  asm_setupresult(as, ir, ci);
  asm_gencall(as, ci, args);
}

static int asm_swapops(ASMState *as, IRIns *ir) {
  IRIns *irl = IR(ir->op1);
  IRIns *irr = IR(ir->op2);
  lua_assert(ra_noreg(irr->r));
  if (!irm_iscomm(lj_ir_mode[ir->o])) {
    return 0;  /* Can't swap non-commutative operations. */
  }
  if (irref_isk(ir->op2)) {
    return 0;  /* Don't swap constants to the left. */
  }
  if (ra_hasreg(irl->r)) {
    return 1;  /* Swap if left already has a register. */
  }
  if (ra_samehint(ir->r, irr->r)) {
    return 1;  /* Swap if dest and right have matching hints. */
  }
  if (as->curins > as->loopref) {  /* In variant part? */
    if (ir->op2 < as->loopref && !irt_isphi(irr->t)) {
      return 0;  /* Keep invariants on the right. */
    }
    if (ir->op1 < as->loopref && !irt_isphi(irl->t)) {
      return 1;  /* Swap invariants to the right. */
    }
  }
  if (opisfusableload(irl->o)) {
    return 1;  /* Swap fusable loads to the right. */
  }
  return 0;  /* Otherwise don't swap. */
}

static void asm_fparith(ASMState *as, IRIns *ir, x86Op xo) {
  IRRef lref = ir->op1;
  IRRef rref = ir->op2;
  RegSet allow = RSET_FPR;
  Reg dest;
  Reg right = IR(rref)->r;
  if (ra_hasreg(right)) {
    rset_clear(allow, right);
    ra_noweak(as, right);
  }
  dest = ra_dest(as, ir, allow);
  if (lref == rref) {
    right = dest;
  } else if (ra_noreg(right)) {
    if (asm_swapops(as, ir)) {
      IRRef tmp = lref; lref = rref; rref = tmp;
    }
    right = asm_fuseload(as, rref, rset_clear(allow, dest));
  }
  emit_mrm(as, xo, dest, right);
  ra_left(as, dest, lref);
}

static void asm_intarith(ASMState *as, IRIns *ir, x86Arith xa) {
  IRRef lref = ir->op1;
  IRRef rref = ir->op2;
  RegSet allow = RSET_GPR;
  Reg dest, right;
  int32_t k = 0;
  uj_opt_x86_fold_test_rr(as);
  right = IR(rref)->r;
  if (ra_hasreg(right)) {
    rset_clear(allow, right);
    ra_noweak(as, right);
  }
  dest = ra_dest(as, ir, allow);
  if (lref == rref) {
    right = dest;
  } else if (ra_noreg(right) && !asm_isk32(as, rref, &k)) {
    if (asm_swapops(as, ir)) {
      IRRef tmp = lref; lref = rref; rref = tmp;
    }
    right = asm_fuseloadm(as, rref, rset_clear(allow, dest), irt_is64(ir->t));
  }
  if (irt_isguard(ir->t)) { /* For IR_ADDOV etc. */
    asm_guardcc(as, CC_O);
  }
  if (xa != XOg_X_IMUL) {
    if (ra_hasreg(right)) {
      emit_mrm(as, XO_ARITH(xa), REX_64IR(ir, dest), right);
    } else {
      emit_gri(as, XG_ARITHi(xa), REX_64IR(ir, dest), k);
    }
  } else if (ra_hasreg(right)) {  /* IMUL r, mrm. */
    emit_mrm(as, XO_IMUL, REX_64IR(ir, dest), right);
  } else {  /* IMUL r, r, k. */
    /* NYI: use lea/shl/add/sub (FOLD only does 2^k) depending on CPU. */
    Reg left = asm_fuseloadm(as, lref, RSET_GPR, irt_is64(ir->t));
    x86Op xo;
    if (checki8(k)) {
      emit_i8(as, k); xo = XO_IMULi8;
    } else {
      emit_i32(as, k); xo = XO_IMULi;
    }
    emit_mrm(as, xo, REX_64IR(ir, dest), left);
    return;
  }
  ra_left(as, dest, lref);
}

/* LEA is really a 4-operand ADD with an independent destination register,
** up to two source registers and an immediate. One register can be scaled
** by 1, 2, 4 or 8. This can be used to avoid moves or to fuse several
** instructions.
**
** Currently only a few common cases are supported:
** - 3-operand ADD:    y = a+b; y = a+k   with a and b already allocated
** - Left ADD fusion:  y = (a+b)+k; y = (a+k)+b
** - Right ADD fusion: y = a+(b+k)
** The ommited variants have already been reduced by FOLD.
**
** There are more fusion opportunities, like gathering shifts or joining
** common references. But these are probably not worth the trouble, since
** array indexing is not decomposed and already makes use of all fields
** of the ModRM operand.
*/
static int asm_lea(ASMState *as, IRIns *ir) {
  IRIns *irl = IR(ir->op1);
  IRIns *irr = IR(ir->op2);
  RegSet allow = RSET_GPR;
  Reg dest;
  as->mrm.base = as->mrm.idx = RID_NONE;
  as->mrm.scale = XM_SCALE1;
  as->mrm.ofs = 0;
  if (ra_hasreg(irl->r)) {
    rset_clear(allow, irl->r);
    ra_noweak(as, irl->r);
    as->mrm.base = irl->r;
    if (irref_isk(ir->op2) || ra_hasreg(irr->r)) {
      /* The PHI renaming logic does a better job in some cases. */
      if (ra_hasreg(ir->r) &&
          ((irt_isphi(irl->t) && as->phireg[ir->r] == ir->op1) ||
           (irt_isphi(irr->t) && as->phireg[ir->r] == ir->op2)))
        return 0;
      if (irref_isk(ir->op2)) {
        as->mrm.ofs = irr->i;
      } else {
        rset_clear(allow, irr->r);
        ra_noweak(as, irr->r);
        as->mrm.idx = irr->r;
      }
    } else {
      return 0;
    }
  } else {
    return 0;
  }
  dest = ra_dest(as, ir, allow);
  emit_mrm(as, XO_LEA, dest, RID_MRM);
  return 1;  /* Success. */
}

static void asm_add(ASMState *as, IRIns *ir) {
  if (irt_isnum(ir->t)) {
    asm_fparith(as, ir, XO_ADDSD);
  } else if (as->flagmcp == as->mcp || irt_is64(ir->t) || !asm_lea(as, ir)) {
    asm_intarith(as, ir, XOg_ADD);
  }
}

static void asm_neg_not(ASMState *as, IRIns *ir, x86Group3 xg) {
  Reg dest = ra_dest(as, ir, RSET_GPR);
  emit_rr(as, XO_GROUP3, REX_64IR(ir, xg), dest);
  ra_left(as, dest, ir->op1);
}

static void asm_min_max(ASMState *as, IRIns *ir, int cc) {
  Reg right, dest = ra_dest(as, ir, RSET_GPR);
  IRRef lref = ir->op1, rref = ir->op2;
  if (irref_isk(rref)) { lref = rref; rref = ir->op1; }
  right = ra_alloc1(as, rref, rset_exclude(RSET_GPR, dest));
  emit_rr(as, XO_CMOV + (cc<<24), REX_64IR(ir, dest), right);
  emit_rr(as, XO_CMP, REX_64IR(ir, dest), right);
  ra_left(as, dest, lref);
}

static void asm_bitswap(ASMState *as, IRIns *ir) {
  Reg dest = ra_dest(as, ir, RSET_GPR);
  as->mcp = emit_op(XO_BSWAP + ((dest&7) << 24),
                    REX_64IR(ir, 0), dest, 0, as->mcp, 1);
  ra_left(as, dest, ir->op1);
}

static void asm_bitshift(ASMState *as, IRIns *ir, x86Shift xs) {
  IRRef rref = ir->op2;
  IRIns *irr = IR(rref);
  Reg dest;
  if (irref_isk(rref)) {  /* Constant shifts. */
    int shift;
    dest = ra_dest(as, ir, RSET_GPR);
    shift = irr->i & (irt_is64(ir->t) ? 63 : 31);
    switch (shift) {
    case 0: break;
    case 1: emit_rr(as, XO_SHIFT1, REX_64IR(ir, xs), dest); break;
    default: emit_shifti(as, REX_64IR(ir, xs), dest, shift); break;
    }
  } else {  /* Variable shifts implicitly use register cl (i.e. ecx). */
    Reg right;
    dest = ra_dest(as, ir, rset_exclude(RSET_GPR, RID_ECX));
    if (dest == RID_ECX) {
      dest = ra_scratch(as, rset_exclude(RSET_GPR, RID_ECX));
      emit_rr(as, XO_MOV, RID_ECX, dest);
    }
    right = irr->r;
    if (ra_noreg(right)) {
      right = ra_allocref(as, rref, RID2RSET(RID_ECX));
    } else if (right != RID_ECX) {
      ra_scratch(as, RID2RSET(RID_ECX));
    }
    emit_rr(as, XO_SHIFTcl, REX_64IR(ir, xs), dest);
    ra_noweak(as, right);
    if (right != RID_ECX) {
      emit_rr(as, XO_MOV, RID_ECX, right);
    }
  }
  ra_left(as, dest, ir->op1);
  /*
  ** Note: avoid using the flags resulting from a shift or rotate!
  ** All of them cause a partial flag stall, except for r,1 shifts
  ** (but not rotates). And a shift count of 0 leaves the flags unmodified.
  */
}

/* -- Comparisons --------------------------------------------------------- */

/* Virtual flags for unordered FP comparisons. */
#define VCC_U   0x1000          /* Unordered. */
#define VCC_P   0x2000          /* Needs extra CC_P branch. */
#define VCC_S   0x4000          /* Swap avoids CC_P branch. */
#define VCC_PS  (VCC_P|VCC_S)

/* Map of comparisons to flags. ORDER IR. */
#define COMPFLAGS(ci, cin, cu, cf)      ((ci)+((cu)<<4)+((cin)<<8)+(cf))
static const uint16_t asm_compmap[IR_ABC+1] = {
  /*                 signed non-eq unsigned flags */
  /* LT  */ COMPFLAGS(CC_GE, CC_G,  CC_AE, VCC_PS),
  /* GE  */ COMPFLAGS(CC_L,  CC_L,  CC_B,  0),
  /* LE  */ COMPFLAGS(CC_G,  CC_G,  CC_A,  VCC_PS),
  /* GT  */ COMPFLAGS(CC_LE, CC_L,  CC_BE, 0),
  /* ULT */ COMPFLAGS(CC_AE, CC_A,  CC_AE, VCC_U),
  /* UGE */ COMPFLAGS(CC_B,  CC_B,  CC_B,  VCC_U|VCC_PS),
  /* ULE */ COMPFLAGS(CC_A,  CC_A,  CC_A,  VCC_U),
  /* UGT */ COMPFLAGS(CC_BE, CC_B,  CC_BE, VCC_U|VCC_PS),
  /* EQ  */ COMPFLAGS(CC_NE, CC_NE, CC_NE, VCC_P),
  /* NE  */ COMPFLAGS(CC_E,  CC_E,  CC_E,  VCC_U|VCC_P),
  /* ABC */ COMPFLAGS(CC_BE, CC_B,  CC_BE, VCC_U|VCC_PS)  /* Same as UGT. */
};

/* FP and integer comparisons. */
static void asm_comp(ASMState *as, IRIns *ir, uint32_t cc) {
  if (irt_isnum(ir->t)) {
    IRRef lref = ir->op1;
    IRRef rref = ir->op2;
    Reg left, right;
    const MCode *l_around;
    /*
    ** An extra CC_P branch is required to preserve ordered/unordered
    ** semantics for FP comparisons. This can be avoided by swapping
    ** the operands and inverting the condition (except for EQ and UNE).
    ** So always try to swap if possible.
    **
    ** Another option would be to swap operands to achieve better memory
    ** operand fusion. But it's unlikely that this outweighs the cost
    ** of the extra branches.
    */
    if (cc & VCC_S) {  /* Swap? */
      IRRef tmp = lref; lref = rref; rref = tmp;
      cc ^= (VCC_PS|(5<<4));  /* A <-> B, AE <-> BE, PS <-> none */
    }
    left = ra_alloc1(as, lref, RSET_FPR);
    right = asm_fuseload(as, rref, rset_exclude(RSET_FPR, left));
    l_around = uj_emit_label(as);
    asm_guardcc(as, cc >> 4);
    if (cc & VCC_P) {  /* Extra CC_P branch required? */
      if (!(cc & VCC_U)) {
        asm_guardcc(as, CC_P);  /* Branch to exit for ordered comparisons. */
      } else if (l_around != as->invmcp) {
        uj_emit_jccs(as, CC_P, l_around);  /* Branch around for unordered. */
      } else {
        /* Patched to mcloop by asm_loop_fixup. */
        as->loopinv = 2;
        if (as->realign) {
          uj_emit_jccs(as, CC_P, as->mcp);
        } else {
          uj_emit_jccn(as, CC_P, as->mcp);
        }
      }
    }
    emit_mrm(as, XO_UCOMISD, left, right);
  } else {
    IRRef lref = ir->op1, rref = ir->op2;
    IROp leftop = (IROp)(IR(lref)->o);
    Reg r64 = REX_64IR(ir, 0);
    int32_t imm = 0;
    lua_assert(irt_is64(ir->t) || irt_isint(ir->t) ||
               irt_isu32(ir->t) || irt_isaddr(ir->t) || irt_isu8(ir->t));
    /* Swap constants (only for ABC) and fusable loads to the right. */
    if (irref_isk(lref) || (!irref_isk(rref) && opisfusableload(leftop))) {
      if ((cc & 0xc) == 0xc) { cc ^= 0x53; } /* L <-> G, LE <-> GE */
      else if ((cc & 0xa) == 0x2) { cc ^= 0x55; } /* A <-> B, AE <-> BE */
      lref = ir->op2; rref = ir->op1;
    }
    if (asm_isk32(as, rref, &imm)) {
      IRIns *irl = IR(lref);
      /* Check wether we can use test ins. Not for unsigned, since CF=0. */
      int usetest = (imm == 0 && (cc & 0xa) != 0x2);
      if (usetest && irl->o == IR_BAND && irl+1 == ir && !ra_used(irl)) {
        /* Combine comp(BAND(ref, r/imm), 0) into test mrm, r/imm. */
        Reg right, left = RID_NONE;
        RegSet allow = RSET_GPR;
        if (!asm_isk32(as, irl->op2, &imm)) {
          left = ra_alloc1(as, irl->op2, allow);
          rset_clear(allow, left);
        } else {  /* Try to Fuse IRT_I8/IRT_U8 loads, too. See below. */
          IRIns *irll = IR(irl->op1);
          if (opisfusableload((IROp)irll->o) &&
              (irt_isi8(irll->t) || irt_isu8(irll->t))) {
            IRType1 origt = irll->t;  /* Temporarily flip types. */
            irll->t.irt = (irll->t.irt & ~IRT_TYPE) | IRT_INT;
            as->curins--;  /* Skip to BAND to avoid failing in noconflict(). */
            right = asm_fuseload(as, irl->op1, RSET_GPR);
            as->curins++;
            irll->t = origt;
            if (right != RID_MRM) { goto test_nofuse; }
            /* Fusion succeeded, emit test byte mrm, imm8. */
            asm_guardcc(as, cc);
            emit_i8(as, (imm & 0xff));
            emit_mrm(as, XO_GROUP3b, XOg_TEST, RID_MRM);
            return;
          }
        }
        as->curins--;  /* Skip to BAND to avoid failing in noconflict(). */
        right = asm_fuseloadm(as, irl->op1, allow, r64);
        as->curins++;  /* Undo the above. */
      test_nofuse:
        asm_guardcc(as, cc);
        if (ra_noreg(left)) {
          emit_i32(as, imm);
          emit_mrm(as, XO_GROUP3, r64 + XOg_TEST, right);
        } else {
          emit_mrm(as, XO_TEST, r64 + left, right);
        }
      } else {
        Reg left;
        if (opisfusableload((IROp)irl->o) &&
            ((irt_isu8(irl->t) && checku8(imm)) ||
             ((irt_isi8(irl->t) || irt_isi16(irl->t)) && checki8(imm)) ||
             (irt_isu16(irl->t) && checku16(imm) && checki8((int16_t)imm)))) {
          /* Only the IRT_INT case is fused by asm_fuseload.
          ** The IRT_I8/IRT_U8 loads and some IRT_I16/IRT_U16 loads
          ** are handled here.
          ** Note that cmp word [mem], imm16 should not be generated,
          ** since it has a length-changing prefix. Compares of a word
          ** against a sign-extended imm8 are ok, however.
          */
          IRType1 origt = irl->t;  /* Temporarily flip types. */
          irl->t.irt = (irl->t.irt & ~IRT_TYPE) | IRT_INT;
          left = asm_fuseload(as, lref, RSET_GPR);
          irl->t = origt;
          if (left == RID_MRM) {  /* Fusion succeeded? */
            if (irt_isu8(irl->t) || irt_isu16(irl->t)) {
              cc >>= 4;  /* Need unsigned compare. */
            }
            asm_guardcc(as, cc);
            emit_i8(as, imm);
            emit_mrm(as, (irt_isi8(origt) || irt_isu8(origt)) ?
                         XO_ARITHib : XO_ARITHiw8, r64 + XOg_CMP, RID_MRM);
            return;
          }  /* Otherwise handle register case as usual. */
        } else {
          left = asm_fuseloadm(as, lref, RSET_GPR, r64);
        }
        asm_guardcc(as, cc);
        if (usetest && left != RID_MRM) {
          /* Use test r,r instead of cmp r,0. */
          x86Op xo = XO_TEST;
          if (irt_isu8(ir->t)) {
            lua_assert(ir->o == IR_EQ || ir->o == IR_NE);
            xo = XO_TESTb;
            if (!rset_test(RSET_RANGE(RID_EAX, RID_EBX+1), left)) {
              left |= FORCE_REX;
            }
          }
          emit_rr(as, xo, r64 + left, left);
          if (irl+1 == ir) { /* Referencing previous ins? */
            as->flagmcp = as->mcp;  /* Set flag to drop test r,r if possible. */
          }
        } else {
          emit_gmrmi(as, XG_ARITHi(XOg_CMP), r64 + left, imm);
        }
      }
    } else {
      Reg left = ra_alloc1(as, lref, RSET_GPR);
      Reg right = asm_fuseloadm(as, rref, rset_exclude(RSET_GPR, left), r64);
      asm_guardcc(as, cc);
      emit_mrm(as, XO_CMP, r64 + left, right);
    }
  }
}

/* -- Handling argument buffer -------------------------------------------- */

/*
 * Assuming that base holds an address of an array of TValue's, fills the n-th
 * TValue with the payload returned by ref.
 *
 * TODO:
 * * It probably makes sense to merge this with asm_stack_restore;
 * * One may created specialized versions of IR_TVARG. For example, a TVARG
 *   may be the only IR referencing some SLOAD. In this case, a variant of
 *   TVARG may fetch the argument directly from the Lua stack, and a guarded
 *   SLOAD may be completely eliminated. The same applies to other loads
 *   (with relevant metatable guards in case of working with tables).
 */
static void asm_payload2tv(ASMState *as, IRRef ref, RegSet allow, Reg base, size_t n)
{
  const int32_t ofs = (int32_t)(n * sizeof(TValue));
  const int32_t ofs_tag = (int32_t)(offsetof(TValue, value_tag));
  IRIns *ir = IR(ref);
  IRType1 t = ir->t;

  if (irt_isnum(t) || irt_isfloat(t)) {
    Reg src = ra_alloc1(as, ref, RSET_FPR);

    emit_rmro(as, irt_isnum(t)? XO_MOVSDto : XO_MOVSSto, src, base, ofs);
    emit_movmroi(as, base, ofs + ofs_tag, LJ_TNUMX);
  } else if (irt_ispri(t) || irt_isaddr(t)) {
    /* Emit the payload. */
    if (!irref_isk(ref)) {
      Reg src = ra_alloc1(as, ref, allow);

      emit_movtomro(as, REX_64IR(ir, src), base, ofs);
    } else if (!irt_ispri(t)) {
      emit_movmroi64(as, base, ofs, ir->i);
    }

    /* Emit value_tag. */
    emit_movmroi(as, base, ofs + ofs_tag, irt_toitype(t));
  } else if (irt_isinteger(t) || irt_isu32(t) || irt_isint64(t)) {
    /* Integers that should be widened. */
    Reg src = ra_alloc1(as, ref, allow);
    Reg tmp = ra_scratch(as, RSET_FPR);

    emit_rmro(as, XO_MOVSDto, tmp, base, ofs);
    emit_movmroi(as, base, ofs + ofs_tag, LJ_TNUMX);
    emit_rr(as, XO_CVTSI2SD, REX_64IR(ir, tmp), src);
  } else {
    lua_assert(0);
  }

  checkmclim(as);
}

static void asm_tvargf(ASMState *as, IRIns *ir)
{
  Reg dest;
  RegSet allow;
  IRIns *ir_tvarg = ir;
  int32_t n = (int32_t)ir->op2 - 1;

  lua_assert((int32_t)ir->op2 <= ARGBUF_MAX_SIZE);

  dest = ra_dest(as, ir, RSET_GPR);
  allow = rset_exclude(RSET_GPR, dest);

  /*
   * After all manipulations we simply refetch the pointer to argbuf to dest.
   * NB! Do not try to employ RID_INTERNAL as a temporary register here, as
   * it may be needed by the register allocator for things like
   * rematerialization of constants.
   */
  emit_getgl(as, dest | REX_64, argbuf);

  while (n--) {
    ir_tvarg = IR(ir_tvarg->op1);

    lua_assert((IROp)ir_tvarg->o == IR_TVARG);
    asm_payload2tv(as, ir_tvarg->op2, allow, dest, n + 1);
  }

  lua_assert((IROp)ir_tvarg->o == IR_TVARG);
  asm_payload2tv(as, ir_tvarg->op1, allow, dest, 0);

  emit_rmro(as, XO_MOV, dest | REX_64, dest, offsetof(struct argbuf, base));
  emit_movmroi(as, dest, offsetof(struct argbuf, n), (int32_t)ir->op2);
  emit_getgl(as, dest | REX_64, argbuf);
}

/* -- Stack handling ------------------------------------------------------ */

/* Check Lua stack size for overflow. Use exit handler as fallback. */
static void asm_stack_check(ASMState *as, BCReg topslot,
                            IRIns *irp, RegSet allow, ExitNo exitno) {
  /* Try to get an unused temp. register, otherwise spill/restore eax. */
  Reg pbase = irp ? irp->r : RID_BASE;
  Reg r = allow ? rset_pickbot(allow) : RID_EAX;
  uj_emit_jccn(as, CC_B, exitstub_addr(as->J, exitno));
  if (allow == RSET_EMPTY) { /* Restore temp. register. */
    emit_rmro(as, XO_MOV, r|REX_64, RID_ESP, 0);
  } else {
    ra_modified(as, r);
  }
  emit_gri(as, XG_ARITHi(XOg_CMP), r|REX_64, (int32_t)(sizeof(TValue)*topslot));
  if (ra_hasreg(pbase) && pbase != r) {
    emit_rr(as, XO_ARITH(XOg_SUB), r|REX_64, pbase);
  } else {
    emit_rr(as, XO_ARITH(XOg_SUB), r|REX_64, RID_INTERNAL);
    emit_getgl(as, RID_INTERNAL|REX_64, jit_base);
  }
  emit_rmro(as, XO_MOV, r|REX_64, r, offsetof(lua_State, maxstack));
  emit_getgl(as, r|REX_64, jit_L);
  if (allow == RSET_EMPTY) { /* Spill temp. register. */
    emit_rmro(as, XO_MOVto, r|REX_64, RID_ESP, 0);
  }
}

/* Restore Lua stack from on-trace state. */
static void asm_stack_restore(ASMState *as, SnapShot *snap) {
  SnapEntry *map = &as->T->snapmap[snap->mapofs];
  SnapEntry *flinks = &as->T->snapmap[snap_nextofs(as->T, snap)-2];
  size_t n, nent = snap->nent;
  /* Store the value of all modified slots to the Lua stack. */
  for (n = 0; n < nent; n++) {
    SnapEntry sn = map[n];
    BCReg s = snap_slot(sn);
    int32_t ofs = ((int32_t)sizeof(TValue))*((int32_t)s-1);
    IRRef ref = snap_ref(sn);
    IRIns *ir = IR(ref);
    if ((sn & SNAP_NORESTORE)) { continue; }
    if (irt_isnum(ir->t)) {
      Reg src = ra_alloc1(as, ref, RSET_FPR);
      emit_rmro(as, XO_MOVSDto, src, RID_BASE, ofs);
      emit_movmroi(as, RID_BASE, ofs+(offsetof(TValue, value_tag)), LJ_TNUMX);
    } else if (sn & SNAP_LOW) {
      Reg src = ra_alloc1(as, ref, rset_exclude(RSET_GPR, RID_BASE));
      lua_assert(irt_size(ir->t) <= 4);
      lua_assert(irt_isfp(ir->t) || irt_isinteger(ir->t) || irt_isu32(ir->t));
      emit_rmro(as, XO_MOVto, src, RID_BASE, ofs);
      emit_movmroi(as, RID_BASE, ofs+(offsetof(TValue, value_tag)), LJ_TNUMX);
    } else {
      lua_assert(irt_ispri(ir->t) || irt_isaddr(ir->t));

      /* Emit the payload. */
      if (!irref_isk(ref)) {
        Reg src = ra_alloc1(as, ref, rset_exclude(RSET_GPR, RID_BASE));
        emit_movtomro(as, REX_64IR(ir, src), RID_BASE, ofs); // !!
        if (!(REX_64IR(ir, src) & REX_64)) { emit_movmroi(as, RID_BASE, ofs+4, 0); }
      } else if (!irt_ispri(ir->t)) {
        emit_movmroi64(as, RID_BASE, ofs, ir->i);
      }

      /* Emit value_tag/framelink. */
      if ((sn & (SNAP_CONT|SNAP_FRAME))) {
        if (s != 0) { /* Do not overwrite link to previous frame. */
          emit_movmroi64(as, RID_BASE, ofs+(offsetof(TValue, fr.tp)), *(uint64_t *)flinks); flinks -= 2;
        }
      } else {
          emit_movmroi(as, RID_BASE, ofs+(offsetof(TValue, value_tag)), irt_toitype(ir->t));
          // TValue.padding is not set here
      }
    }
    checkmclim(as);
  }
  lua_assert(map + nent == flinks);
}

/* -- GC handling --------------------------------------------------------- */

/* Check GC threshold and do one or more GC steps. */
static void asm_gc_check(ASMState *as) {
  const CCallInfo *ci = &lj_ir_callinfo[IRCALL_lj_gc_step_jit];
  /* global_State *g, size_t steps */
  const IRRef args[2] = {ASMREF_TMP1, ASMREF_TMP2};
  const MCode *l_end;
  Reg tmp;
  ra_evictset(as, RSET_SCRATCH);
  l_end = uj_emit_label(as);
  /* Exit trace if in GCSatomic or GCSfinalize. Avoids syncing GC objects. */
  asm_test_retval_zero(as, CC_NE); /* side-exit if returned non-zero */
  asm_gencall(as, ci, args);
  tmp = ra_releasetmp(as, ASMREF_TMP1);
  emit_loada(as, tmp, J2G(as->J));
  emit_loadi(as, ra_releasetmp(as, ASMREF_TMP2), as->gcsteps);
  /* Jump around GC step if GC total < GC threshold. */
  uj_emit_jccs(as, CC_B, l_end);
  emit_opgl(as, XO_ARITH(XOg_CMP), tmp|REX_64, gc.threshold);
  emit_getgl(as, tmp|REX_64, mem.metrics.total);
  as->gcsteps = 0;
  checkmclim(as);
}

/* -- Loop handling ------------------------------------------------------- */

/* Fixup the loop branch. */
static void asm_loop_fixup(ASMState *as) {
  MCode *p = as->mctop;
  MCode *target = as->mcp;
  if (as->realign) {  /* Realigned loops use short jumps. */
    as->realign = NULL;  /* Stop another retry. */
    lua_assert(((intptr_t)target & 15) == 0);
    if (as->loopinv) {  /* Inverted loop branch? */
      p -= 5;
      p[0] = XI_JMP;
      lua_assert(target - p >= -128);
      p[-1] = (MCode)(target - p);  /* Patch sjcc. */
      if (as->loopinv == 2) {
        p[-3] = (MCode)(target - p + 2);  /* Patch opt. short jp. */
      }
    } else {
      lua_assert(target - p >= -128);
      p[-1] = (MCode)(int8_t)(target - p);  /* Patch short jmp. */
      p[-2] = XI_JMPs;
    }
  } else {
    MCode *newloop;
    p[-5] = XI_JMP;
    if (as->loopinv) {  /* Inverted loop branch? */
      /* asm_guardcc already inverted the jcc and patched the jmp. */
      p -= 5;
      newloop = target+4;
      *(int32_t *)(p-4) = (int32_t)(target - p);  /* Patch jcc. */
      if (as->loopinv == 2) {
        *(int32_t *)(p-10) = (int32_t)(target - p + 6);  /* Patch opt. jp. */
        newloop = target+8;
      }
    } else {  /* Otherwise just patch jmp. */
      *(int32_t *)(p-4) = (int32_t)(target - p);
      newloop = target+3;
    }
    /* Realign small loops and shorten the loop branch. */
    if (newloop >= p - 128) {
      as->realign = newloop;  /* Force a retry and remember alignment. */
      as->curins = as->stopins;  /* Abort asm_trace now. */
      as->T->nins = as->orignins;  /* Remove any added renames. */
    }
  }
}

/* -- Head of trace ------------------------------------------------------- */

/* Coalesce BASE register for a root trace. */
static void asm_head_root_base(ASMState *as) {
  IRIns *ir = IR(REF_BASE);
  Reg r = ir->r;
  if (ra_hasreg(r)) {
    ra_free(as, r);
    if (rset_test(as->modset, r) || irt_ismarked(ir->t)) {
      ir->r = RID_INIT;  /* No inheritance for modified BASE register. */
    }
    if (r != RID_BASE) {
      emit_rr(as, XO_MOV, r|REX_64, RID_BASE);
    }
  }
}

/* Coalesce or reload BASE register for a side trace. */
static RegSet asm_head_side_base(ASMState *as, IRIns *irp, RegSet allow) {
  IRIns *ir = IR(REF_BASE);
  Reg r = ir->r;
  if (ra_hasreg(r)) {
    ra_free(as, r);
    if (rset_test(as->modset, r) || irt_ismarked(ir->t)) {
      ir->r = RID_INIT;  /* No inheritance for modified BASE register. */
    }
    if (irp->r == r) {
      rset_clear(allow, r);  /* Mark same BASE register as coalesced. */
    } else if (ra_hasreg(irp->r) && rset_test(as->freeset, irp->r)) {
      rset_clear(allow, irp->r);
      emit_rr(as, XO_MOV, r|REX_64, irp->r);  /* Move from coalesced parent reg. */
    } else {
      emit_getgl(as, r|REX_64, jit_base);  /* Otherwise reload BASE. */
    }
  }
  return allow;
}

/* -- Tail of trace ------------------------------------------------------- */

/* Fixup the tail code. */
static void asm_tail_fixup(ASMState *as, TraceNo lnk) {
  /* Note: don't use as->mcp swap + emit_*: emit_op overwrites more bytes. */
  MCode *p = as->mctop;
  MCode *target, *q;
  int32_t spadj = as->T->spadjust;
  if (spadj == 0) {
    p -= 7;
  } else {
    MCode *p1;
    /* Patch stack adjustment. */
    if (checki8(spadj)) {
      p -= 3;
      p1 = p-6;
      *p1 = (MCode)spadj;
    } else {
      p1 = p-9;
      *(int32_t *)p1 = spadj;
    }
    p1[-3] = 0x48;
    p1[-2] = (MCode)(checki8(spadj) ? XI_ARITHi8 : XI_ARITHi);
    p1[-1] = MODRM(XM_REG, XOg_ADD, RID_ESP);
  }
  /* Patch exit branch. */
UJ_PEDANTIC_OFF /* casting a function ptr to object pointer type */
  target = lnk ? traceref(as->J, lnk)->mcode : (MCode *)lj_vm_exit_interp;
UJ_PEDANTIC_ON
  *(int32_t *)(p-4) = uj_emit_rel32(p, target);
  p[-5] = XI_JMP;
  /* Drop unused mcode tail. Fill with NOPs to make the prefetcher happy. */
  for (q = as->mctop-1; q >= p; q--) {
    *q = XI_NOP;
  }
  as->mctop = p;
}

/* Prepare tail of code. */
static void asm_tail_prep(ASMState *as) {
  MCode *p = as->mctop;
  /* Realign and leave room for backwards loop branch or exit branch. */
  if (as->realign) {
    int i = ((int)(intptr_t)as->realign) & 15;
    /* Fill unused mcode tail with NOPs to make the prefetcher happy. */
    while (i-- > 0) {
      *--p = XI_NOP;
    }
    as->mctop = p;
    p -= (as->loopinv ? 5 : 2);  /* Space for short/near jmp. */
  } else {
    p -= 5;  /* Space for exit branch (near jmp). */
  }
  if (as->loopref) {
    as->invmcp = as->mcp = p;
  } else {
    /* Leave room for ESP adjustment: add esp, imm or lea esp, [esp+imm] */
    as->mcp = p - 7;
    as->invmcp = NULL;
  }
}

/* -- Instruction dispatch ------------------------------------------------ */

/* Assemble a single instruction. */
static void asm_ir(ASMState *as, IRIns *ir) {
  switch ((IROp)ir->o) {
  /* Miscellaneous ops. */
  case IR_LOOP:
    asm_loop(as);
    break;
  case IR_NOP:
  case IR_XBAR:
    lua_assert(!ra_used(ir));
    break;
  case IR_USE:
    ra_alloc1(as, ir->op1, irt_isfp(ir->t) ? RSET_FPR : RSET_GPR);
    break;
  case IR_PHI:
    asm_phi(as, ir);
    break;
  case IR_GCSTEP:
    asm_gcstep(as, ir);
    break;

  /* Guarded assertions. */
  case IR_LT:
  case IR_GE:
  case IR_LE:
  case IR_GT:
  case IR_ULT:
  case IR_UGE:
  case IR_ULE:
  case IR_UGT:
  case IR_EQ:
  case IR_NE:
  case IR_ABC:
    asm_comp(as, ir, asm_compmap[ir->o]);
    break;

  case IR_RETF:
    asm_retf(as, ir);
    break;

  /* Bit ops. */
  case IR_BNOT: asm_neg_not(as, ir, XOg_NOT); break;
  case IR_BSWAP: asm_bitswap(as, ir); break;
  case IR_BAND: asm_intarith(as, ir, XOg_AND); break;
  case IR_BOR:  asm_intarith(as, ir, XOg_OR); break;
  case IR_BXOR: asm_intarith(as, ir, XOg_XOR); break;
  case IR_BSHL: asm_bitshift(as, ir, XOg_SHL); break;
  case IR_BSHR: asm_bitshift(as, ir, XOg_SHR); break;
  case IR_BSAR: asm_bitshift(as, ir, XOg_SAR); break;
  case IR_BROL: asm_bitshift(as, ir, XOg_ROL); break;
  case IR_BROR: asm_bitshift(as, ir, XOg_ROR); break;

  /* Arithmetic ops. */
  case IR_ADD: asm_add(as, ir); break;
  case IR_SUB:
    if (irt_isnum(ir->t)) {
      asm_fparith(as, ir, XO_SUBSD);
    } else { /* Note: no need for LEA trick here. i-k is encoded as i+(-k). */
      asm_intarith(as, ir, XOg_SUB);
    }
    break;
  case IR_MUL:
    if (irt_isnum(ir->t)) {
      asm_fparith(as, ir, XO_MULSD);
    } else {
      asm_intarith(as, ir, XOg_X_IMUL);
    }
    break;
  case IR_DIV:
#if LJ_HASFFI
    if (!irt_isnum(ir->t)) {
      asm_arith64(as, ir, irt_isi64(ir->t) ? IRCALL_lj_carith_divi64 :
                                             IRCALL_lj_carith_divu64);
    } else
#endif
    {
      asm_fparith(as, ir, XO_DIVSD);
    }
    break;
  case IR_MOD:
#if LJ_HASFFI
    if (!irt_isint(ir->t)) {
      asm_arith64(as, ir, irt_isi64(ir->t) ? IRCALL_lj_carith_modi64 :
                                             IRCALL_lj_carith_modu64);
    } else
#endif
    {
      asm_intmod(as, ir);
    }
    break;

  case IR_NEG:
    if (irt_isnum(ir->t)) {
      asm_fparith(as, ir, XO_XORPS);
    } else {
      asm_neg_not(as, ir, XOg_NEG);
    }
    break;
  case IR_ABS: asm_fparith(as, ir, XO_ANDPS); break;

  case IR_MIN:
    if (irt_isnum(ir->t)) {
      asm_fparith(as, ir, XO_MINSD);
    } else {
      asm_min_max(as, ir, CC_G);
    }
    break;
  case IR_MAX:
    if (irt_isnum(ir->t)) {
      asm_fparith(as, ir, XO_MAXSD);
    }
    else {
      asm_min_max(as, ir, CC_L);
    }
    break;

  case IR_FPMATH: case IR_ATAN2: case IR_LDEXP:
    asm_fpmath(as, ir);
    break;
  case IR_FPAND:
    asm_fparith(as, ir, XO_ANDPS);
    break;
  case IR_POW:
#if LJ_HASFFI
    if (!irt_isnum(ir->t)) {
      asm_arith64(as, ir, irt_isi64(ir->t) ? IRCALL_lj_carith_powi64 :
                                             IRCALL_lj_carith_powu64);
    } else
#endif
    {
      asm_fppowi(as, ir);
    }
    break;

  /* Overflow-checking arithmetic ops. Note: don't use LEA here! */
  case IR_ADDOV:
    asm_intarith(as, ir, XOg_ADD);
    break;
  case IR_SUBOV:
    asm_intarith(as, ir, XOg_SUB);
    break;
  case IR_MULOV:
    asm_intarith(as, ir, XOg_X_IMUL);
    break;

  /* Memory references. */
  case IR_AREF:
    asm_aref(as, ir);
    break;
  case IR_HREF:
    asm_href(as, ir);
    break;
  case IR_HREFK:
    asm_hrefk(as, ir);
    break;
  case IR_NEWREF:
    asm_newref(as, ir);
    break;
  case IR_UREFO:
  case IR_UREFC:
    asm_uref(as, ir);
    break;
  case IR_FREF:
    asm_fref(as, ir);
    break;
  case IR_STRREF:
    asm_strref(as, ir);
    break;
  case IR_LREF:
    asm_lref(as, ir);
    break;

  /* Loads and stores. */
  case IR_ALOAD:
  case IR_HLOAD:
  case IR_HKLOAD:
  case IR_ULOAD:
  case IR_VLOAD:
  case IR_TVLOAD:
    asm_ahuvload(as, ir);
    break;
  case IR_FLOAD:
  case IR_XLOAD:
    asm_fxload(as, ir);
    break;
  case IR_SLOAD:
    asm_sload(as, ir);
    break;

  case IR_ASTORE:
  case IR_HSTORE:
  case IR_USTORE:
    asm_ahustore(as, ir);
    break;
  case IR_FSTORE:
  case IR_XSTORE:
    asm_fxstore(as, ir);
    break;

  /* Allocations. */
  case IR_SNEW:
  case IR_XSNEW:
    asm_snew(as, ir);
    break;
  case IR_TNEW:
    asm_tnew(as, ir);
    break;
  case IR_TDUP:
    asm_tdup(as, ir);
    break;
  case IR_CNEW:
  case IR_CNEWI:
    asm_cnew(as, ir);
    break;

  /* Write barriers. */
  case IR_TBAR:
    asm_tbar(as, ir);
    break;
  case IR_OBAR:
    asm_obar(as, ir);
    break;

  /* Buffer operations. */
  case IR_BUFHDR:
    asm_bufhdr(as, ir);
    break;
  case IR_BUFPUT:
    asm_bufput(as, ir);
    break;
  case IR_BUFSTR:
    asm_bufstr(as, ir);
    break;

  /* Type conversions. */
  case IR_TOBIT:
    asm_tobit(as, ir);
    break;
  case IR_CONV:
    asm_conv(as, ir);
    break;
  case IR_TOSTR:
    asm_tostr(as, ir);
    break;
  case IR_STRTO:
    asm_strto(as, ir);
    break;

  /* Calls. */
  case IR_CALLN:
  case IR_CALLL:
  case IR_CALLS:
    asm_call(as, ir);
    break;
  case IR_CALLXS:
    asm_callx(as, ir);
    break;
  case IR_CARG:
    break;

  /* Handling argument buffer */
  case IR_TVARG:
    break; /* no op; IR -> mcode will happen in the finalizer (TVARGF) */
  case IR_TVARGF:
    asm_tvargf(as, ir);
    break;

  default:
    lj_trace_err_info_op(as->J, LJ_TRERR_NYIIR, (int32_t)(ir->o));
    break;
  }
}

/* -- Trace setup --------------------------------------------------------- */

/* Ensure there are enough stack slots for call arguments. */
static Reg asm_setup_call_slots(ASMState *as, IRIns *ir, const CCallInfo *ci) {
  IRRef args[CCI_NARGS_MAX*2];
  int nslots;
  asm_collectargs(as, ir, ci, args);
  nslots = asm_count_call_slots(as, ci, args);
  if (nslots > as->evenspill) { /* Leave room for args in stack slots. */
    as->evenspill = nslots;
  }
  return irt_isfp(ir->t) ? REGSP_HINT(RID_FPRET) : REGSP_HINT(RID_RET);
}

/* Target-specific setup. */
static void asm_setup_target(ASMState *as) {
  asm_exitstub_setup(as, as->T->nsnap);
}

/* -- Trace patching ------------------------------------------------------ */

#include "utils/x86_inslen.h"
#ifdef NDEBUG
#define lj_asm_inslen x86_inslen
#else
#include "udis86.h"
#define UDIS_PSEUDO_BUFFER_SIZE 128 /* Should be enough. */
static uint32_t lj_asm_inslen(const uint8_t *p) {
  uint32_t inslen;
  uint32_t ud_inslen;
  ud_t ud_obj;
  ud_init(&ud_obj);
  ud_set_mode(&ud_obj, 64);
  ud_set_pc(&ud_obj, (uint64_t)(void *)p);
  ud_set_input_buffer(&ud_obj, p, UDIS_PSEUDO_BUFFER_SIZE);
  inslen = x86_inslen(p);
  ud_inslen = ud_disassemble(&ud_obj);
  lua_assert(inslen == ud_inslen);
  return inslen;
}
#undef UDIS_PSEUDO_BUFFER_SIZE
#endif // NDEBUG

#define OP_LEN_TEST_RAX_RAX 3
#define OP_LEN_CALL_REL32   5

static int asm_is_test_rax_rax(const MCode *p) {
  const unsigned char *instr = (unsigned char *)p;

  return instr[0] == 0x48     /* rex prefix */
      && instr[1] == XI_TEST  /* call */
      && instr[2] == 0xC0;    /* eax, eax */
}

static int asm_is_call_gc_step(const MCode *instr) {
  if (*instr != XI_CALL) {
    return 0;
  }

  const void *call_target = instr + *(int32_t *)(instr + 1) + OP_LEN_CALL_REL32;

UJ_PEDANTIC_OFF /* comparison of a function ptr with void* */
  if (call_target != &lj_gc_step_jit) {
    return 0;
  }
UJ_PEDANTIC_ON

  return 1;
}

static int asm_is_gc_step_sideexit(const MCode *p) {
  /* Check for 'test rax, rax' instruction' */
  p -= OP_LEN_TEST_RAX_RAX;

  if(!asm_is_test_rax_rax(p)) {
    return 0;
  }

  /* Check for 'call lj_gc_step_jit' instruction */
  p -= OP_LEN_CALL_REL32;
  if (!asm_is_call_gc_step(p)) {
    return 0;
  }

  return 1;
}

/* Patch exit jumps of existing machine code to a new target. */
void lj_asm_patchexit(jit_State *J, GCtrace *T, ExitNo exitno, MCode *target) {
  MCode *p = T->mcode;
  MCode *mcarea = lj_mcode_patch_start(J, p);
  size_t len = T->szmcode;
  MCode *px = exitstub_addr(J, exitno) - 6;
  MCode *pe = p+len-6;
  void *stateaddr = (void *)(&J2G(J)->vmstate);
  if (len > 5 && p[len-5] == XI_JMP && p+len-6 + *(int32_t *)(p+len-4) == px) {
    *(int32_t *)(p+len-4) = uj_emit_rel32(p+len, target);
  }
  /* Do not patch parent exit for a stack check. Skip beyond vmstate update.
  ** This piece of code is very fragile. It depends on how emit_setvmstate is
  ** implemented in both signature scanning (if condition) and spacing (how many
  ** bytes are skipped. Note that both signature and skipping rely on the fact
  ** that REX prefix of loada is already scanned (it's at p[-1]).
  */
  for (; p < pe; p += lj_asm_inslen(p)) {
    if (p[1] == (MCode)(XI_MOVri+(RID_INTERNAL&7)) && *(void **)(p+2) == stateaddr) {
      break;
    }
  }

  lua_assert(p < pe);

  for (; p < pe; p += lj_asm_inslen(p)) {
    if ((*(uint16_t *)p & 0xf0ff) == 0x800f && p + *(int32_t *)(p+2) == px) {
      if (!asm_is_gc_step_sideexit(p)) {
        uj_emit_jcc_fixup(p, target);
      }
    }
  }

  lj_mcode_sync(T->mcode, T->mcode + T->szmcode);
  lj_mcode_patch_finish(J, mcarea);
}

