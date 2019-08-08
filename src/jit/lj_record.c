/*
 * Trace recorder (bytecode -> SSA IR).
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "lj_obj.h"

#if LJ_HASJIT

#include "lj_tab.h"
#include "uj_dispatch.h"
#include "uj_meta.h"
#include "uj_mtab.h"
#include "lj_frame.h"
#include "uj_proto.h"
#if LJ_HASFFI
#include "ffi/lj_ctype.h"
#endif
#include "lj_bc.h"
#include "uj_ff.h"
#include "jit/lj_ir.h"
#include "jit/lj_jit.h"
#include "jit/lj_ircall.h"
#include "jit/lj_iropt.h"
#include "jit/lj_trace.h"
#include "jit/lj_record.h"
#include "jit/uj_record_indexed.h"
#include "jit/lj_ffrecord.h"
#include "jit/lj_snap.h"
#include "lj_vm.h"
#include "uj_hotcnt.h"

/* Some local macros to save typing. Undef'd at the end. */
#define IR(ref)                 (&J->cur.ir[(ref)])

/* Pass IR on to next optimization in chain (FOLD). */
#define emitir(ot, a, b)        (lj_ir_set(J, (ot), (a), (b)), lj_opt_fold(J))

/* Emit raw IR without passing through optimizations. */
#define emitir_raw(ot, a, b)    (lj_ir_set(J, (ot), (a), (b)), lj_ir_emit(J))

/* -- Sanity checks ------------------------------------------------------- */

#ifndef NDEBUG
/* Sanity check the whole IR -- sloooow. */
static void rec_check_ir(jit_State *J)
{
  IRRef i, nins = J->cur.nins, nk = J->cur.nk;
  lua_assert(nk <= REF_BIAS && nins >= REF_BIAS && nins < 65536);
  for (i = nins-1; i >= nk; i--) {
    IRIns *ir = IR(i);
    uint32_t mode = lj_ir_mode[ir->o];
    IRRef op1 = ir->op1;
    IRRef op2 = ir->op2;
    switch (irm_op1(mode)) {
    case IRMnone: lua_assert(op1 == 0); break;
    case IRMref: lua_assert(op1 >= nk);
      lua_assert(i >= REF_BIAS ? op1 < i : op1 > i); break;
    case IRMlit: break;
    case IRMcst: lua_assert(i < REF_BIAS); continue;
    }
    switch (irm_op2(mode)) {
    case IRMnone: lua_assert(op2 == 0); break;
    case IRMref: lua_assert(op2 >= nk);
      lua_assert(i >= REF_BIAS ? op2 < i : op2 > i); break;
    case IRMlit: break;
    case IRMcst: lua_assert(0); break;
    }
    if (ir->prev) {
      lua_assert(ir->prev >= nk);
      lua_assert(i >= REF_BIAS ? ir->prev < i : ir->prev > i);
      lua_assert(ir->o == IR_NOP || IR(ir->prev)->o == ir->o);
    }
  }
}

/* Compare stack slots and frames of the recorder and the VM. */
static void rec_check_slots(jit_State *J)
{
  BCReg s, nslots = J->baseslot + J->maxslot;
  int32_t depth = 0;
  const TValue *base = J->L->base - J->baseslot;
  lua_assert(J->baseslot >= 1 && J->baseslot < LJ_MAX_JSLOTS);
  lua_assert(J->baseslot == 1 || (J->slot[J->baseslot-1] & TREF_FRAME));
  lua_assert(nslots < LJ_MAX_JSLOTS);
  for (s = 0; s < nslots; s++) {
    TRef tr = J->slot[s];
    if (tr) {
      const TValue *tv = &base[s];
      IRRef ref = tref_ref(tr);
      IRIns *ir;
      lua_assert(ref >= J->cur.nk && ref < J->cur.nins);
      ir = IR(ref);
      lua_assert(irt_t(ir->t) == tref_t(tr));
      if (s == 0) {
        lua_assert(tref_isfunc(tr));
      } else if ((tr & TREF_FRAME)) {
        GCfunc *fn = gco2func(frame_gc(tv));
        BCReg delta = (BCReg)(tv - frame_prev(tv));
        lua_assert(tref_isfunc(tr));
        if (tref_isk(tr)) lua_assert(fn == ir_kfunc(ir));
        lua_assert(s > delta ? (J->slot[s-delta] & TREF_FRAME) : (s == delta));
        depth++;
      } else if ((tr & TREF_CONT)) {
        lua_assert(ir_kptr(ir) == (void *)tv->gcr);
        lua_assert((J->slot[s+1] & TREF_FRAME));
        depth++;
      } else {
        if (tvisnum(tv))
          lua_assert(tref_isnumber(tr));  /* Could be IRT_INT etc., too. */
        else
          lua_assert(itype2irt(tv) == tref_type(tr));
        if (tref_isk(tr)) {  /* Compare constants. */
          TValue tvk;
          lj_ir_kvalue(J->L, &tvk, ir);
          if (!(tvisnum(&tvk) && tvisnan(&tvk)))
            lua_assert(uj_obj_equal(tv, &tvk));
          else
            lua_assert(tvisnum(tv) && tvisnan(tv));
        }
      }
    }
  }
  lua_assert(J->framedepth == depth);
}
#endif

/* -- Type handling and specialization ------------------------------------ */

/* Note: these functions return tagged references (TRef). */

/* Specialize a slot to a specific type. Note: slot can be negative! */
static TRef sloadt(jit_State *J, int32_t slot, IRType t, int mode)
{
  /* Caller may set IRT_GUARD in t. */
  TRef ref = emitir_raw(IRT(IR_SLOAD, t), (int32_t)J->baseslot+slot, mode);
  J->base[slot] = ref;
  return ref;
}

/* Specialize a slot to the runtime type. Note: slot can be negative! */
static TRef sload(jit_State *J, int32_t slot)
{
  IRType t = itype2irt(&J->L->base[slot]);
  TRef ref = emitir_raw(IRTG(IR_SLOAD, t), (int32_t)J->baseslot+slot,
                        IRSLOAD_TYPECHECK);
  if (irtype_ispri(t)) ref = TREF_PRI(t);  /* Canonicalize primitive refs. */
  J->base[slot] = ref;
  return ref;
}

/* Get TRef from slot. Load slot and specialize if not done already. */
static TRef rec_getslot(jit_State *J, int32_t slot)
{
  TRef tr = J->base[slot] ? J->base[slot] : sload(J, slot);

  lua_assert(tr);

  return lj_opt_movtv_rec_hint(J, tr);
}

/* Get TRef for current function. */
static TRef getcurrf(jit_State *J)
{
  if (J->base[-1])
    return lj_opt_movtv_rec_hint(J, J->base[-1]);
  lua_assert(J->baseslot == 1);
  return sloadt(J, -1, IRT_FUNC, IRSLOAD_READONLY);
}

/* Compare for raw object equality.
** Returns 0 if the objects are the same.
** Returns 1 if they are different, but the same type.
** Returns 2 for two different types.
** Comparisons between primitives always return 1 -- no caller cares about it.
*/
int lj_record_objcmp(jit_State *J, TRef a, TRef b, const TValue *av, const TValue *bv)
{
  int diff = !uj_obj_equal(av, bv);
  if (!tref_isk2(a, b)) {  /* Shortcut, also handles primitives. */
    IRType ta = tref_isinteger(a) ? IRT_INT : tref_type(a);
    IRType tb = tref_isinteger(b) ? IRT_INT : tref_type(b);
    if (ta != tb) {
      /* Widen mixed number/int comparisons to number/number comparison. */
      if (ta == IRT_INT && tb == IRT_NUM) {
        a = emitir(IRTN(IR_CONV), a, IRCONV_NUM_INT);
        ta = IRT_NUM;
      } else if (ta == IRT_NUM && tb == IRT_INT) {
        b = emitir(IRTN(IR_CONV), b, IRCONV_NUM_INT);
      } else {
        return 2;  /* Two different types are never equal. */
      }
    }
    emitir(IRTG(diff ? IR_NE : IR_EQ, ta), a, b);
  }
  return diff;
}

/* Constify a value. Returns 0 for non-representable object types. */
TRef lj_record_constify(jit_State *J, const TValue *o)
{
  if (tvisgcv(o))
    return lj_ir_kgc(J, gcV(o), itype2irt(o));
  else if (tvisnum(o))
    return lj_ir_knumint(J, numV(o));
  else if (tvisbool(o))
    return TREF_PRI(itype2irt(o));
  else
    return 0;  /* Can't represent lightuserdata (pointless). */
}

/* -- Record loop ops ----------------------------------------------------- */

/* Loop event. */
typedef enum {
  LOOPEV_LEAVE,         /* Loop is left or not entered. */
  LOOPEV_ENTERLO,       /* Loop is entered with a low iteration count left. */
  LOOPEV_ENTER          /* Loop is entered. */
} LoopEvent;

/* Canonicalize slots: convert integers to numbers. */
static void canonicalize_slots(jit_State *J)
{
  BCReg s;
  for (s = J->baseslot+J->maxslot-1; s >= 1; s--) {
    TRef tr = J->slot[s];
    if (tref_isinteger(tr) && !(tr & TREF_LOW)) {
      IRIns *ir = IR(tref_ref(tr));
      if (!(ir->o == IR_SLOAD && (ir->op2 & IRSLOAD_READONLY)))
        J->slot[s] = emitir(IRTN(IR_CONV), tr, IRCONV_NUM_INT);
    }
  }
}

/* Stop recording. */
static void rec_stop(jit_State *J, TraceLink linktype, TraceNo lnk)
{
  lj_trace_end(J);
  J->cur.linktype = (uint8_t)linktype;
  J->cur.link = (uint16_t)lnk;
  /* Looping back at the same stack level? */
  if (lnk == J->cur.traceno && J->framedepth + J->retdepth == 0) {
    if ((J->flags & JIT_F_OPT_LOOP))  /* Shall we try to create a loop? */
      goto nocanon;  /* Do not canonicalize or we lose the narrowing. */
    if (J->cur.root)  /* Otherwise ensure we always link to the root trace. */
      J->cur.link = J->cur.root;
  }
  canonicalize_slots(J);
nocanon:
  /* Note: all loop ops must set J->pc to the following instruction! */
  lj_snap_add(J);  /* Add loop snapshot. */
  J->needsnap = 0;
  J->mergesnap = 1;  /* In case recording continues. */
}

/* Search bytecode backwards for a int/num constant slot initializer. */
static TRef find_kinit(jit_State *J, const BCIns *endpc, BCReg slot, IRType t)
{
  /* This algorithm is rather simplistic and assumes quite a bit about
  ** how the bytecode is generated. It works fine for FORI initializers,
  ** but it won't necessarily work in other cases (e.g. iterator arguments).
  ** It doesn't do anything fancy, either (like backpropagating MOVs).
  */
  const BCIns *pc, *startpc = proto_bc(J->pt);
  for (pc = endpc-1; pc > startpc; pc--) {
    BCIns ins = *pc;
    BCOp op = bc_op(ins);
    /* First try to find the last instruction that stores to this slot. */
    if (bcmode_a(op) == BCMbase && bc_a(ins) <= slot) {
      return 0;  /* Multiple results, e.g. from a CALL or KNIL. */
    } else if (bcmode_a(op) == BCMdst && bc_a(ins) == slot) {
      if (op == BC_KSHORT || op == BC_KNUM) {  /* Found const. initializer. */
        /* Now try to verify there's no forward jump across it. */
        const BCIns *kpc = pc;
        for (; pc > startpc; pc--)
          if (bc_op(*pc) == BC_JMP) {
            const BCIns *target = pc+bc_j(*pc)+1;
            if (target > kpc && target <= endpc)
              return 0;  /* Conditional assignment. */
          }
        if (op == BC_KSHORT) {
          int32_t k = (int32_t)(int16_t)bc_d(ins);
          return t == IRT_INT ? lj_ir_kint(J, k) : lj_ir_knum(J, (lua_Number)k);
        } else {
          const TValue *tv = proto_knumtv(J->pt, bc_d(ins));
          if (t == IRT_INT) {
            int32_t k = lj_num2int(numV(tv));
            if (numV(tv) == (lua_Number)k)  /* -0 is ok here. */
              return lj_ir_kint(J, k);
            return 0;  /* Type mismatch. */
          } else {
            return lj_ir_knum(J, numV(tv));
          }
        }
      }
      return 0;  /* Non-constant initializer. */
    }
  }
  return 0;  /* No assignment to this slot found? */
}

/* Load and optionally convert a FORI argument from a slot. */
static TRef fori_load(jit_State *J, BCReg slot, IRType t, int mode)
{
  int conv = (t==IRT_INT) ? IRSLOAD_CONVERT : 0;
  return sloadt(J, (int32_t)slot,
                t + (((mode & IRSLOAD_TYPECHECK) ||
                      (conv && t == IRT_INT && !(mode >> 16))) ?
                     IRT_GUARD : 0),
                mode + conv);
}

/* Peek before FORI to find a const initializer. Otherwise load from slot. */
static TRef fori_arg(jit_State *J, const BCIns *fori, BCReg slot,
                     IRType t, int mode)
{
  TRef tr = J->base[slot];
  if (!tr) {
    tr = find_kinit(J, fori, slot, t);
    if (!tr)
      tr = fori_load(J, slot, t, mode);
  }
  return tr;
}

/* Return the direction of the FOR loop iterator.
** It's important to exactly reproduce the semantics of the interpreter.
*/
static int rec_for_direction(const TValue *o)
{
  return (int32_t)o->u32.hi >= 0;
}

/* Simulate the runtime behavior of the FOR loop iterator. */
static LoopEvent rec_for_iter(IROp *op, const TValue *o, int isforl)
{
  lua_Number stopv = numV(&o[FORL_STOP]);
  lua_Number idxv = numV(&o[FORL_IDX]);
  lua_Number stepv = numV(&o[FORL_STEP]);
  if (isforl)
    idxv += stepv;
  if (rec_for_direction(&o[FORL_STEP])) {
    if (idxv <= stopv) {
      *op = IR_LE;
      return idxv + 2*stepv > stopv ? LOOPEV_ENTERLO : LOOPEV_ENTER;
    }
    *op = IR_GT; return LOOPEV_LEAVE;
  } else {
    if (stopv <= idxv) {
      *op = IR_GE;
      return idxv + 2*stepv < stopv ? LOOPEV_ENTERLO : LOOPEV_ENTER;
    }
    *op = IR_LT; return LOOPEV_LEAVE;
  }
}

/* Record checks for FOR loop overflow and step direction. */
static void rec_for_check(jit_State *J, IRType t, int dir,
                          TRef stop, TRef step, int init)
{
  if (!tref_isk(step)) {
    /* Non-constant step: need a guard for the direction. */
    TRef zero = (t == IRT_INT) ? lj_ir_kint(J, 0) : lj_ir_knum_zero(J);
    emitir(IRTG(dir ? IR_GE : IR_LT, t), step, zero);
    /* Add hoistable overflow checks for a narrowed FORL index. */
    if (init && t == IRT_INT) {
      if (tref_isk(stop)) {
        /* Constant stop: optimize check away or to a range check for step. */
        int32_t k = IR(tref_ref(stop))->i;
        if (dir) {
          if (k > 0)
            emitir(IRTGI(IR_LE), step, lj_ir_kint(J, (int32_t)0x7fffffff-k));
        } else {
          if (k < 0)
            emitir(IRTGI(IR_GE), step, lj_ir_kint(J, (int32_t)0x80000000-k));
        }
      } else {
        /* Stop+step variable: need full overflow check. */
        TRef tr = emitir(IRTGI(IR_ADDOV), step, stop);
        emitir(IRTI(IR_USE), tr, 0);  /* ADDOV is weak. Avoid dead result. */
      }
    }
  } else if (init && t == IRT_INT && !tref_isk(stop)) {
    /* Constant step: optimize overflow check to a range check for stop. */
    int32_t k = IR(tref_ref(step))->i;
    k = (int32_t)(dir ? 0x7fffffff : 0x80000000) - k;
    emitir(IRTGI(dir ? IR_LE : IR_GE), stop, lj_ir_kint(J, k));
  }
}

/* Record a FORL instruction. */
static void rec_for_loop(jit_State *J, const BCIns *fori, ScEvEntry *scev,
                         int init)
{
  BCReg ra = bc_a(*fori);
  const TValue *tv = &J->L->base[ra];
  TRef idx = J->base[ra+FORL_IDX];
  IRType t = idx ? tref_type(idx) :
             (init) ? lj_opt_narrow_forl(J, tv) : IRT_NUM;
  int mode = IRSLOAD_INHERIT + IRSLOAD_READONLY;
  TRef stop = fori_arg(J, fori, ra+FORL_STOP, t, mode);
  TRef step = fori_arg(J, fori, ra+FORL_STEP, t, mode);
  int dir = rec_for_direction(&tv[FORL_STEP]);
  lua_assert(bc_op(*fori) == BC_FORI || bc_op(*fori) == BC_JFORI);
  scev->t.irt = t;
  scev->dir = dir;
  scev->stop = tref_ref(stop);
  scev->step = tref_ref(step);
  rec_for_check(J, t, dir, stop, step, init);
  scev->start = tref_ref(find_kinit(J, fori, ra+FORL_IDX, IRT_INT));
  if (!idx)
    idx = fori_load(J, ra+FORL_IDX, t,
                    IRSLOAD_INHERIT + (J->scev.start << 16));
  if (!init)
    J->base[ra+FORL_IDX] = idx = emitir(IRT(IR_ADD, t), idx, step);
  J->base[ra+FORL_EXT] = idx;
  scev->idx = tref_ref(idx);
  scev->pc = fori;
  J->maxslot = ra+FORL_EXT+1;
}

/* Record FORL/JFORL or FORI/JFORI. */
static LoopEvent rec_for(jit_State *J, const BCIns *fori, int isforl)
{
  BCReg ra = bc_a(*fori);
  TValue *tv = &J->L->base[ra];
  TRef *tr = &J->base[ra];
  IROp op;
  LoopEvent ev;
  TRef stop;
  IRType t;
  if (isforl) {  /* Handle FORL/JFORL opcodes. */
    TRef idx = tr[FORL_IDX];
    if (J->scev.pc == fori && tref_ref(idx) == J->scev.idx) {
      t = J->scev.t.irt;
      stop = J->scev.stop;
      idx = emitir(IRT(IR_ADD, t), idx, J->scev.step);
      tr[FORL_EXT] = tr[FORL_IDX] = idx;
    } else {
      ScEvEntry scev;
      rec_for_loop(J, fori, &scev, 0);
      t = scev.t.irt;
      stop = scev.stop;
    }
  } else {  /* Handle FORI/JFORI opcodes. */
    BCReg i;
    uj_meta_for(J->L, tv);
    t = (tref_isint(tr[FORL_IDX])) ? lj_opt_narrow_forl(J, tv) : IRT_NUM;
    for (i = FORL_IDX; i <= FORL_STEP; i++) {
      if (!tr[i]) sload(J, ra+i);
      lua_assert(tref_isnumber_str(tr[i]));
      if (tref_isstr(tr[i]))
        tr[i] = emitir(IRTG(IR_STRTO, IRT_NUM), tr[i], 0);
      if (t == IRT_INT) {
        if (!tref_isinteger(tr[i]))
          tr[i] = emitir(IRTGI(IR_CONV), tr[i], IRCONV_INT_NUM|IRCONV_CHECK);
      } else {
        if (!tref_isnum(tr[i]))
          tr[i] = emitir(IRTN(IR_CONV), tr[i], IRCONV_NUM_INT);
      }
    }
    tr[FORL_EXT] = tr[FORL_IDX];
    stop = tr[FORL_STOP];
    rec_for_check(J, t, rec_for_direction(&tv[FORL_STEP]),
                  stop, tr[FORL_STEP], 1);
  }

  ev = rec_for_iter(&op, tv, isforl);
  if (ev == LOOPEV_LEAVE) {
    J->maxslot = ra+FORL_EXT+1;
    J->pc = fori+1;
  } else {
    J->maxslot = ra;
    J->pc = fori+bc_j(*fori)+1;
  }
  lj_snap_add(J);

  emitir(IRTG(op, t), tr[FORL_IDX], stop);

  if (ev == LOOPEV_LEAVE) {
    J->maxslot = ra;
    J->pc = fori+bc_j(*fori)+1;
  } else {
    J->maxslot = ra+FORL_EXT+1;
    J->pc = fori+1;
  }
  J->needsnap = 1;
  return ev;
}

/* Record ITERL/JITERL and ITRNL/JITRNL */
static LoopEvent rec_iterl(jit_State *J, const BCIns iterins)
{
  BCReg ra = bc_a(iterins);
  BCOp op = bc_op(iterins);
  lua_assert(J->base[ra] != 0);
  if (!tref_isnil(J->base[ra])) {  /* Looping back? */
    if (bc_isiterl(op))
      J->base[ra-1] = J->base[ra];  /* Copy result of ITERC to control var. */
    J->maxslot = ra-1+bc_b(J->pc[-1]);
    J->pc += bc_j(iterins)+1;
    return LOOPEV_ENTER;
  } else {
    J->maxslot = ra-3;
    J->pc++;
    return LOOPEV_LEAVE;
  }
}

/* Record LOOP/JLOOP. Now, that was easy. */
static LoopEvent rec_loop(jit_State *J, BCReg ra)
{
  if (ra < J->maxslot) J->maxslot = ra;
  J->pc++;
  return LOOPEV_ENTER;
}

/* Check if a loop repeatedly failed to trace because it didn't loop back. */
static int innerloopleft(jit_State *J, const BCIns *pc)
{
  ptrdiff_t i;
  for (i = 0; i < PENALTY_SLOTS; i++)
    if (J->penalty[i].pc == pc) {
      if ((J->penalty[i].reason == LJ_TRERR_LLEAVE ||
           J->penalty[i].reason == LJ_TRERR_LINNER) &&
          J->penalty[i].val >= 2*PENALTY_MIN)
        return 1;
      break;
    }
  return 0;
}

/* Handle the case when an interpreted loop op is hit. */
static void rec_loop_interp(jit_State *J, const BCIns *pc, LoopEvent ev)
{
  if (J->parent == 0) {
    if (pc == J->startpc && J->framedepth + J->retdepth == 0) {
      /* Same loop? */
      if (ev == LOOPEV_LEAVE)  /* Must loop back to form a root trace. */
        lj_trace_err(J, LJ_TRERR_LLEAVE);
      rec_stop(J, LJ_TRLINK_LOOP, J->cur.traceno);  /* Looping root trace. */
    } else if (ev != LOOPEV_LEAVE) {  /* Entering inner loop? */
      /* It's usually better to abort here and wait until the inner loop
      ** is traced. But if the inner loop repeatedly didn't loop back,
      ** this indicates a low trip count. In this case try unrolling
      ** an inner loop even in a root trace. But it's better to be a bit
      ** more conservative here and only do it for very short loops.
      */
      if (bc_j(*pc) != -1 && !innerloopleft(J, pc))
        lj_trace_err(J, LJ_TRERR_LINNER);  /* Root trace hit an inner loop. */
      if ((ev != LOOPEV_ENTERLO &&
           J->loopref && J->cur.nins - J->loopref > 24) || --J->loopunroll < 0)
        lj_trace_err(J, LJ_TRERR_LUNROLL);  /* Limit loop unrolling. */
      J->loopref = J->cur.nins;
    }
  } else if (ev != LOOPEV_LEAVE) {  /* Side trace enters an inner loop. */
    J->loopref = J->cur.nins;
    if (--J->loopunroll < 0)
      lj_trace_err(J, LJ_TRERR_LUNROLL);  /* Limit loop unrolling. */
  }  /* Side trace continues across a loop that's left or not entered. */
}

/* Handle the case when an already compiled loop op is hit. */
static void rec_loop_jit(jit_State *J, TraceNo lnk, LoopEvent ev)
{
  if (J->parent == 0) {  /* Root trace hit an inner loop. */
    /* Better let the inner loop spawn a side trace back here. */
    lj_trace_err(J, LJ_TRERR_LINNER);
  } else if (ev != LOOPEV_LEAVE) {  /* Side trace enters a compiled loop. */
    J->instunroll = 0;  /* Cannot continue across a compiled loop op. */
    if (J->pc == J->startpc && J->framedepth + J->retdepth == 0)
      rec_stop(J, LJ_TRLINK_LOOP, J->cur.traceno);  /* Form an extra loop. */
    else
      rec_stop(J, LJ_TRLINK_ROOT, lnk);  /* Link to the loop. */
  }  /* Side trace continues across a loop that's left or not entered. */
}

static void rec_isnext(jit_State *J, BCReg ra)
{
  TRef func = rec_getslot(J, (int32_t)(ra - 3));
  TRef tab = rec_getslot(J, (int32_t)(ra - 2));

  if (tref_isfunc(func) && tref_istab(tab)
      && funcV(&J->L->base[ra - 3])->c.ffid == FF_next_N) {
    TRef ffid = emitir(IRTI(IR_FLOAD), func, IRFL_FUNC_FFID);
    emitir(IRTGI(IR_EQ), ffid, lj_ir_kint(J, FF_next_N));
    sloadt(J, ra - 1, IRT_U32, 0);
  } /* else interpreter will despecialize to BC_JMP */
}

static void rec_itern(jit_State *J, BCReg ra)
{
  uint32_t index;
  TRef trindex;
  GCtab *tab = tabV(&J->L->base[ra - 2]);
  RecordIndex ix;

  memset(&ix, 0, sizeof(ix));
  ix.tab = rec_getslot(J, (int32_t)(ra - 2));
  settabV(J->L, &ix.tabv, tab);

  index = lj_tab_iterate_jit(tab, J->L->base[ra - 1].u32.lo);
  if (index == 0) /* Let's not record a trace without iterations */
    lj_trace_err_info_op(J, LJ_TRERR_NYIBC, BC_ITERN);

  /* Like rec_getslot, but with custom sload type */
  if (J->base[ra - 1])
    trindex = lj_opt_movtv_rec_hint(J, J->base[ra - 1]);
  else
    trindex = sloadt(J, ra - 1, IRT_U32 | IRT_GUARD, IRSLOAD_TYPECHECK);

  trindex = lj_ir_call(J, IRCALL_lj_tab_iterate_jit, ix.tab, trindex);

  if (index <= tab->asize) {
    /*
     * Guard lj_tab_iterate_jit != 0 is not needed, because
     * (0 - 1) will fail unsigned array bounds check
     */
    ix.key = emitir(IRTI(IR_SUB), trindex, lj_ir_kint(J, 1));
    setnumV(&ix.keyv, index - 1);
    ix.val = uj_record_indexed(J, &ix);
  } else {
    const Node *node = &tab->node[index - 1 - tab->asize];

    TRef trhslot;
    TRef trnode = emitir(IRT(IR_FLOAD, IRT_PTR), ix.tab, IRFL_TAB_NODE);
    TRef trasize = emitir(IRTI(IR_FLOAD), ix.tab, IRFL_TAB_ASIZE);

    emitir(IRTG(IR_GT, IRT_U32), trindex, trasize);
    trhslot = emitir(IRTI(IR_SUB), trindex, lj_ir_kint(J, 1));
    trhslot = emitir(IRTI(IR_SUB), trhslot, trasize);
    /* imul is good enough here, see asm_href comments for more info */
    trhslot = emitir(IRTI(IR_MUL), trhslot, lj_ir_kint(J, sizeof(Node)));
    trnode = emitir(IRT(IR_ADD, IRT_U64), trnode, trhslot);
    ix.key = emitir(IRTG(IR_HKLOAD, itype2irt(&node->key)), trnode, 0);
    ix.val = emitir(IRTG(IR_HLOAD, itype2irt(&node->val)), trnode, 0);
  }
  J->base[ra - 1] = TREF_LOW | trindex;
  J->base[ra] = ix.key;
  J->base[ra + 1] = ix.val;
}

/* -- Record calls and returns -------------------------------------------- */

/* Specialize to the runtime value of the called function or its prototype. */
static TRef rec_call_specialize(jit_State *J, GCfunc *fn, TRef tr)
{
  TRef kfunc;
  if (isluafunc(fn)) {
    GCproto *pt = funcproto(fn);
    /* Too many closures created? Probably not a monomorphic function. */
    if (pt->flags >= PROTO_CLC_POLY) {  /* Specialize to prototype instead. */
      TRef trpt = emitir(IRT(IR_FLOAD, IRT_P32), tr, IRFL_FUNC_PC);
      emitir(IRTG(IR_EQ, IRT_P32), trpt, lj_ir_kptr(J, proto_bc(pt)));
      (void)lj_ir_kgc(J, obj2gco(pt), IRT_PROTO);  /* Prevent GC of proto. */
      return tr;
    }
  }
  /* Otherwise specialize to the function (closure) value itself. */
  kfunc = lj_ir_kfunc(J, fn);
  emitir(IRTG(IR_EQ, IRT_FUNC), tr, kfunc);
  return kfunc;
}

/* Record call setup. */
static void rec_call_setup(jit_State *J, BCReg func, ptrdiff_t nargs)
{
  RecordIndex ix;
  TValue *functv = &J->L->base[func];
  TRef *fbase = &J->base[func];
  ptrdiff_t i;
  for (i = 0; i <= nargs; i++)
    (void)rec_getslot(J, (int32_t)(func + i));  /* Ensure func and all args have a reference. */
  if (!tref_isfunc(fbase[0])) {  /* Resolve __call metamethod. */
    ix.tab = fbase[0];
    copyTV(J->L, &ix.tabv, functv);
    if (!lj_record_mm_lookup(J, &ix, MM_call) || !tref_isfunc(ix.mobj))
      lj_trace_err(J, LJ_TRERR_NOMM);
    for (i = ++nargs; i > 0; i--)  /* Shift arguments up. */
      fbase[i] = fbase[i-1];
    fbase[0] = ix.mobj;  /* Replace function. */
    functv = &ix.mobjv;
  }
  fbase[0] = TREF_FRAME | rec_call_specialize(J, funcV(functv), fbase[0]);
  J->maxslot = (BCReg)nargs;
}

/* Record call. */
void lj_record_call(jit_State *J, BCReg func, ptrdiff_t nargs)
{
  rec_call_setup(J, func, nargs);
  /* Bump frame. */
  J->framedepth++;
  J->base += func+1;
  J->baseslot += func+1;
}

/* Record tail call. */
void lj_record_tailcall(jit_State *J, BCReg func, ptrdiff_t nargs)
{
  rec_call_setup(J, func, nargs);
  if (frame_isvarg(J->L->base - 1)) {
    BCReg cbase = (BCReg)frame_delta(J->L->base - 1);
    if (--J->framedepth < 0)
      lj_trace_err(J, LJ_TRERR_NYIRETL);
    J->baseslot -= (BCReg)cbase;
    J->base -= cbase;
    func += cbase;
  }
  /* Move func + args down. */
  memmove(&J->base[-1], &J->base[func], sizeof(TRef)*(J->maxslot+1));
  /* Note: the new TREF_FRAME is now at J->base[-1] (even for slot #0). */
  /* Tailcalls can form a loop, so count towards the loop unroll limit. */
  if (++J->tailcalled > J->loopunroll)
    lj_trace_err(J, LJ_TRERR_LUNROLL);
}

/* Check unroll limits for down-recursion. */
static int check_downrec_unroll(jit_State *J, const GCproto *pt)
{
  IRRef ptref;
  for (ptref = J->chain[IR_KGC]; ptref; ptref = IR(ptref)->prev)
    if (ir_kgc(IR(ptref)) == obj2gco(pt)) {
      int count = 0;
      IRRef ref;
      for (ref = J->chain[IR_RETF]; ref; ref = IR(ref)->prev)
        if (IR(ref)->op1 == ptref)
          count++;
      if (count) {
        if (J->pc == J->startpc) {
          if (count + J->tailcalled > J->param[JIT_P_recunroll])
            return 1;
        } else {
          lj_trace_err(J, LJ_TRERR_DOWNREC);
        }
      }
    }
  return 0;
}

/* Records return to a Lua frame.
** Please note that `frame` is the frame slot of the function we are returning
** *from*. For the frame we are returning *to*, this naturally is a callee.
*/
static void rec_ret_lua_frame(jit_State *J, BCReg rbase, ptrdiff_t gotresults, const TValue *frame) {
  ptrdiff_t i;

  lua_assert(frame_islua(frame));

  /* From the BC_CALL bytecode that resulted in executing our callee we derive
  ** following data about the *caller*:
  **  * Callee's offset in the caller's frame
  **  * Number of slots used by the caller by the time of the call
  ** The same trick is used in the interpreter, see there for more details.
  */
  const BCIns callins = *(frame_pc(frame) - 1);
  const BCReg callee_slot = bc_a(callins);
  const BCReg nslots = callee_slot + 1;

  const ptrdiff_t nresults = bc_b(callins)?
    (ptrdiff_t)bc_b(callins) - 1 : gotresults;

  const GCproto *caller = funcproto(frame_func(frame - nslots));

  if (uj_proto_jit_disabled(caller)) {
    lj_trace_err(J, LJ_TRERR_CJITOFF);
  }

  if (J->framedepth == 0 && J->pt && frame == J->L->base - 1) {
    if (check_downrec_unroll(J, caller)) {
      J->maxslot = (BCReg)(rbase + gotresults);
      lj_snap_purge(J);
      rec_stop(J, LJ_TRLINK_DOWNREC, J->cur.traceno);  /* Down-recursion. */
      return;
    }
    lj_snap_add(J);
  }

  for (i = 0; i < nresults; i++) { /* Adjust results. */
    J->base[i - 1] = i < gotresults ? J->base[rbase + i] : TREF_NIL;
  }

  J->maxslot = callee_slot + (BCReg)nresults;

  if (J->framedepth > 0) {
    /* Return to a frame that is part of the trace. */
    lua_assert(J->baseslot > nslots);
    J->framedepth--;
    J->base -= nslots;
    J->baseslot -= nslots;
  } else if (J->parent == 0 && !bc_isret(bc_op(J->cur.startins))) {
    /* Return to lower frame would leave the loop in a root trace. */
    lj_trace_err(J, LJ_TRERR_LLEAVE);
  } else if (J->needsnap) {
    /* Tailcalled to ff with side-effects. No way to insert snapshot here. */
    lj_trace_err(J, LJ_TRERR_NYIRETL);
  } else {
    /* Return to lower frame that originally was *not* the part of the trace.
    ** Guard for the target we return to.
    */
    TRef *jit_frame = J->base - 1;

    if ((J->flags & JIT_F_OPT_NORETL)) {
      lj_trace_err(J, LJ_TRERR_NORETL);
    }

    lua_assert(J->baseslot == 1);

    const TRef trpt = lj_ir_kgc(J, obj2gco(caller), IRT_PROTO);
    const TRef trpc = lj_ir_kptr(J, (void *)frame_pc(frame));

    emitir(IRTG(IR_RETF, IRT_P32), trpt, trpc);

    J->retdepth++;
    J->needsnap = 1;

    /* Shift result slots up and clear the slots of the caller's frame
    ** which become accessible only now.
    */
    memmove(J->base + callee_slot, jit_frame, sizeof(TRef) * nresults);
    memset(jit_frame, 0, sizeof(TRef) * nslots);
  }
}

/* Record return. */
void lj_record_ret(jit_State *J, BCReg rbase, ptrdiff_t gotresults)
{
  TValue *frame = J->L->base - 1;
  ptrdiff_t i;
  for (i = 0; i < gotresults; i++)
    (void)rec_getslot(J, (int32_t)(rbase + i));  /* Ensure all results have a reference. */
  while (frame_ispcall(frame)) {  /* Immediately resolve pcall() returns. */
    BCReg cbase = (BCReg)frame_delta(frame);
    if (--J->framedepth <= 0)
      lj_trace_err(J, LJ_TRERR_NYIRETL);
    lua_assert(J->baseslot > 1);
    gotresults++;
    rbase += cbase;
    J->baseslot -= (BCReg)cbase;
    J->base -= cbase;
    J->base[--rbase] = TREF_TRUE;  /* Prepend true to results. */
    frame = frame_prevd(frame);
  }
  /* Return to lower frame via interpreter for unhandled cases. */
  if (J->framedepth == 0 && J->pt && bc_isret(bc_op(*J->pc)) &&
       (!frame_islua(frame) ||
        (J->parent == 0 && !bc_isret(bc_op(J->cur.startins))))) {
    /* NYI: specialize to frame type and return directly, not via RET*. */
    for (i = 0; i < (ptrdiff_t)rbase; i++)
      J->base[i] = 0;  /* Purge dead slots. */
    J->maxslot = rbase + (BCReg)gotresults;
    rec_stop(J, LJ_TRLINK_RETURN, 0);  /* Return to interpreter. */
    return;
  }
  if (frame_isvarg(frame)) {
    BCReg cbase = (BCReg)frame_delta(frame);
    if (--J->framedepth < 0)  /* NYI: return of vararg func to lower frame. */
      lj_trace_err(J, LJ_TRERR_NYIRETL);
    lua_assert(J->baseslot > 1);
    rbase += cbase;
    J->baseslot -= (BCReg)cbase;
    J->base -= cbase;
    frame = frame_prevd(frame);
  }
  if (frame_islua(frame)) {  /* Return to Lua frame. */
    rec_ret_lua_frame(J, rbase, gotresults, frame);
  } else if (frame_iscont(frame)) {  /* Return to continuation frame. */
    ASMFunction cont = frame_contf(frame);
    BCReg cbase = (BCReg)frame_delta(frame);
    if ((J->framedepth -= 2) < 0)
      lj_trace_err(J, LJ_TRERR_NYIRETL);
    J->baseslot -= (BCReg)cbase;
    J->base -= cbase;
    J->maxslot = cbase-2;
    if (cont == lj_cont_ra) {
      /* Copy result to destination slot. */
      BCReg dst = bc_a(*(frame_contpc(frame)-1));
      J->base[dst] = gotresults ? J->base[cbase+rbase] : TREF_NIL;
      if (dst >= J->maxslot) J->maxslot = dst+1;
    } else if (cont == lj_cont_nop) {
      /* Nothing to do here. */
    } else if (cont == lj_cont_cat) {
      lua_assert(0);
    } else {
      /* Result type already specialized. */
      lua_assert(cont == lj_cont_condf || cont == lj_cont_condt);
    }
  } else {
    lj_trace_err(J, LJ_TRERR_NYIRETL);  /* NYI: handle return to C frame. */
  }
  lua_assert(J->baseslot >= 1);
}

/* -- Metamethod handling ------------------------------------------------- */

/* Prepare to record call to metamethod. */
BCReg lj_record_mm_prep(jit_State *J, ASMFunction cont)
{
  BCReg s, top = curr_proto(J->L)->framesize;
  TRef trcont;
  setcont(&J->L->base[top], cont);
  trcont = lj_ir_kptr(J, (void *)((int64_t)cont - (int64_t)lj_vm_asm_begin));
  J->base[top] = trcont | TREF_CONT;
  J->framedepth++;
  for (s = J->maxslot; s < top; s++)
    J->base[s] = 0;  /* Clear frame gap to avoid resurrecting previous refs. */
  return top+1;
}

/* Record metamethod lookup. */
int lj_record_mm_lookup(jit_State *J, RecordIndex *ix, enum MMS mm)
{
  RecordIndex mix;
  GCtab *mt;
  if (tref_istab(ix->tab)) {
    mt = tabV(&ix->tabv)->metatable;
    mix.tab = emitir(IRT(IR_FLOAD, IRT_TAB), ix->tab, IRFL_TAB_META);
  } else if (tref_isudata(ix->tab)) {
    int udtype = udataV(&ix->tabv)->udtype;
    mt = udataV(&ix->tabv)->metatable;
    /* The metatables of special userdata objects are treated as immutable. */
    if (udtype != UDTYPE_USERDATA) {
      const TValue *mo;
      if (LJ_HASFFI && udtype == UDTYPE_FFI_CLIB) {
        /* Specialize to the C library namespace object. */
        emitir(IRTG(IR_EQ, IRT_P32), ix->tab, lj_ir_kptr(J, udataV(&ix->tabv)));
      } else {
        /* Specialize to the type of userdata. */
        TRef tr = emitir(IRT(IR_FLOAD, IRT_U8), ix->tab, IRFL_UDATA_UDTYPE);
        emitir(IRTGI(IR_EQ), tr, lj_ir_kint(J, udtype));
      }
  immutable_mt:
      mo = lj_tab_getstr(mt, uj_meta_name(J2G(J), mm));
      if (!mo || tvisnil(mo))
        return 0;  /* No metamethod. */
      /* Treat metamethod or index table as immutable, too. */
      if (!(tvisfunc(mo) || tvistab(mo)))
        lj_trace_err(J, LJ_TRERR_BADTYPE);
      copyTV(J->L, &ix->mobjv, mo);
      ix->mobj = lj_ir_kgc(J, gcV(mo), tvisfunc(mo) ? IRT_FUNC : IRT_TAB);
      ix->mtv = mt;
      ix->mt = TREF_NIL;  /* Dummy value for comparison semantics. */
      return 1;  /* Got metamethod or index table. */
    }
    mix.tab = emitir(IRT(IR_FLOAD, IRT_TAB), ix->tab, IRFL_UDATA_META);
  } else {
    /* Specialize to base metatable. Must flush mcode in lua_setmetatable(). */
    mt = uj_mtab_get_for_otype(J2G(J), &ix->tabv);
    if (mt == NULL) {
      ix->mt = TREF_NIL;
      return 0;  /* No metamethod. */
    }
    /* The cdata metatable is treated as immutable. */
    if (LJ_HASFFI && tref_iscdata(ix->tab)) goto immutable_mt;
    ix->mt = mix.tab = lj_ir_ktab(J, mt);
    goto nocheck;
  }
  ix->mt = mt ? mix.tab : TREF_NIL;
  emitir(IRTG(mt ? IR_NE : IR_EQ, IRT_TAB), mix.tab, lj_ir_knull(J, IRT_TAB));
  if (!mt && !tref_isk(ix->tab))
    ir_sethint(IR(tref_ref(ix->tab)), IRH_TAB_NOMETAGUARD);
nocheck:
  if (mt) {
    GCstr *mmstr = uj_meta_name(J2G(J), mm);
    const TValue *mo = lj_tab_getstr(mt, mmstr);
    if (mo && !tvisnil(mo))
      copyTV(J->L, &ix->mobjv, mo);
    ix->mtv = mt;
    settabV(J->L, &mix.tabv, mt);
    setstrV(J->L, &mix.keyv, mmstr);
    mix.key = lj_ir_kstr(J, mmstr);
    mix.val = 0;
    mix.idxchain = 0;
    ix->mobj = uj_record_indexed(J, &mix);
    return !tref_isnil(ix->mobj);  /* 1 if metamethod found, 0 if not. */
  }
  return 0;  /* No metamethod. */
}

/* Record call to arithmetic metamethod. */
static TRef rec_mm_arith(jit_State *J, RecordIndex *ix, enum MMS mm)
{
  /* Set up metamethod call first to save ix->tab and ix->tabv. */
  BCReg func = lj_record_mm_prep(J, lj_cont_ra);
  TRef *base = J->base + func;
  TValue *basev = J->L->base + func;
  base[1] = ix->tab; base[2] = ix->key;
  copyTV(J->L, basev+1, &ix->tabv);
  copyTV(J->L, basev+2, &ix->keyv);
  if (!lj_record_mm_lookup(J, ix, mm)) {  /* Lookup mm on 1st operand. */
    if (mm != MM_unm) {
      ix->tab = ix->key;
      copyTV(J->L, &ix->tabv, &ix->keyv);
      if (lj_record_mm_lookup(J, ix, mm))  /* Lookup mm on 2nd operand. */
        goto ok;
    }
    lj_trace_err(J, LJ_TRERR_NOMM);
  }
ok:
  base[0] = ix->mobj;
  copyTV(J->L, basev+0, &ix->mobjv);
  lj_record_call(J, func, MM_NARG_ARITH);
  return 0;  /* No result yet. */
}

/* Record call to __len metamethod. */
static TRef rec_mm_len(jit_State *J, TRef tr, TValue *tv)
{
  RecordIndex ix;
  ix.tab = tr;
  copyTV(J->L, &ix.tabv, tv);
  if (lj_record_mm_lookup(J, &ix, MM_len)) {
    BCReg func = lj_record_mm_prep(J, lj_cont_ra);
    TRef *base = J->base + func;
    TValue *basev = J->L->base + func;
    base[0] = ix.mobj; copyTV(J->L, basev+0, &ix.mobjv);
    base[1] = tr; copyTV(J->L, basev+1, tv);
#if LJ_52
    base[2] = tr; copyTV(J->L, basev+2, tv);
#else
    base[2] = TREF_NIL; setnilV(basev+2);
#endif
    lj_record_call(J, func, uj_mm_narg[MM_len]);
  } else {
    if (LJ_52 && tref_istab(tr))
      return lj_ir_call(J, IRCALL_lj_tab_len, tr);
    lj_trace_err(J, LJ_TRERR_NOMM);
  }
  return 0;  /* No result yet. */
}

/* Call a comparison metamethod. */
static void rec_mm_callcomp(jit_State *J, RecordIndex *ix, int op)
{
  BCReg func = lj_record_mm_prep(J, (op&1) ? lj_cont_condf : lj_cont_condt);
  TRef *base = J->base + func;
  TValue *tv = J->L->base + func;
  base[0] = ix->mobj; base[1] = ix->val; base[2] = ix->key;
  copyTV(J->L, tv+0, &ix->mobjv);
  copyTV(J->L, tv+1, &ix->valv);
  copyTV(J->L, tv+2, &ix->keyv);
  lj_record_call(J, func, MM_NARG_COMP);
}

/* Record call to equality comparison metamethod (for tab and udata only). */
static void rec_mm_equal(jit_State *J, RecordIndex *ix, int op)
{
  ix->tab = ix->val;
  copyTV(J->L, &ix->tabv, &ix->valv);
  if (lj_record_mm_lookup(J, ix, MM_eq)) {  /* Lookup mm on 1st operand. */
    const TValue *bv;
    TRef mo1 = ix->mobj;
    TValue mo1v;
    copyTV(J->L, &mo1v, &ix->mobjv);
    /* Avoid the 2nd lookup and the objcmp if the metatables are equal. */
    bv = &ix->keyv;
    if (tvistab(bv) && tabV(bv)->metatable == ix->mtv) {
      TRef mt2 = emitir(IRT(IR_FLOAD, IRT_TAB), ix->key, IRFL_TAB_META);
      emitir(IRTG(IR_EQ, IRT_TAB), mt2, ix->mt);
    } else if (tvisudata(bv) && udataV(bv)->metatable == ix->mtv) {
      TRef mt2 = emitir(IRT(IR_FLOAD, IRT_TAB), ix->key, IRFL_UDATA_META);
      emitir(IRTG(IR_EQ, IRT_TAB), mt2, ix->mt);
    } else {  /* Lookup metamethod on 2nd operand and compare both. */
      ix->tab = ix->key;
      copyTV(J->L, &ix->tabv, bv);
      if (!lj_record_mm_lookup(J, ix, MM_eq) ||
          lj_record_objcmp(J, mo1, ix->mobj, &mo1v, &ix->mobjv))
        return;
    }
    rec_mm_callcomp(J, ix, op);
  }
}

/* Record call to ordered comparison metamethods (for arbitrary objects). */
static void rec_mm_comp(jit_State *J, RecordIndex *ix, int op)
{
  ix->tab = ix->val;
  copyTV(J->L, &ix->tabv, &ix->valv);
  while (1) {
    enum MMS mm = (op & 2) ? MM_le : MM_lt;  /* Try __le + __lt or only __lt. */
#if LJ_52
    if (!lj_record_mm_lookup(J, ix, mm)) {  /* Lookup mm on 1st operand. */
      ix->tab = ix->key;
      copyTV(J->L, &ix->tabv, &ix->keyv);
      if (!lj_record_mm_lookup(J, ix, mm))  /* Lookup mm on 2nd operand. */
        goto nomatch;
    }
    rec_mm_callcomp(J, ix, op);
    return;
#else
    if (lj_record_mm_lookup(J, ix, mm)) {  /* Lookup mm on 1st operand. */
      const TValue *bv;
      TRef mo1 = ix->mobj;
      TValue mo1v;
      copyTV(J->L, &mo1v, &ix->mobjv);
      /* Avoid the 2nd lookup and the objcmp if the metatables are equal. */
      bv = &ix->keyv;
      if (tvistab(bv) && tabV(bv)->metatable == ix->mtv) {
        TRef mt2 = emitir(IRT(IR_FLOAD, IRT_TAB), ix->key, IRFL_TAB_META);
        emitir(IRTG(IR_EQ, IRT_TAB), mt2, ix->mt);
      } else if (tvisudata(bv) && udataV(bv)->metatable == ix->mtv) {
        TRef mt2 = emitir(IRT(IR_FLOAD, IRT_TAB), ix->key, IRFL_UDATA_META);
        emitir(IRTG(IR_EQ, IRT_TAB), mt2, ix->mt);
      } else {  /* Lookup metamethod on 2nd operand and compare both. */
        ix->tab = ix->key;
        copyTV(J->L, &ix->tabv, bv);
        if (!lj_record_mm_lookup(J, ix, mm) ||
            lj_record_objcmp(J, mo1, ix->mobj, &mo1v, &ix->mobjv))
          goto nomatch;
      }
      rec_mm_callcomp(J, ix, op);
      return;
    }
#endif
  nomatch:
    /* Lookup failed. Retry with  __lt and swapped operands. */
    if (!(op & 2)) break;  /* Already at __lt. Interpreter will throw. */
    ix->tab = ix->key; ix->key = ix->val; ix->val = ix->tab;
    copyTV(J->L, &ix->tabv, &ix->keyv);
    copyTV(J->L, &ix->keyv, &ix->valv);
    copyTV(J->L, &ix->valv, &ix->tabv);
    op ^= 3;
  }
}

#if LJ_HASFFI
/* Setup call to cdata comparison metamethod. */
static void rec_mm_comp_cdata(jit_State *J, RecordIndex *ix, int op, enum MMS mm)
{
  lj_snap_add(J);
  if (tref_iscdata(ix->val)) {
    ix->tab = ix->val;
    copyTV(J->L, &ix->tabv, &ix->valv);
  } else {
    lua_assert(tref_iscdata(ix->key));
    ix->tab = ix->key;
    copyTV(J->L, &ix->tabv, &ix->keyv);
  }
  lj_record_mm_lookup(J, ix, mm);
  rec_mm_callcomp(J, ix, op);
}
#endif

/* -- Indexed access ------------------------------------------------------ */

/* Record bounds-check. */
void lj_record_idx_abc(jit_State *J, TRef asizeref, TRef ikey, uint32_t asize)
{
  /* Try to emit invariant bounds checks. */
  if ((J->flags & (JIT_F_OPT_LOOP|JIT_F_OPT_ABC)) ==
      (JIT_F_OPT_LOOP|JIT_F_OPT_ABC)) {
    IRRef ref = tref_ref(ikey);
    IRIns *ir = IR(ref);
    int32_t ofs = 0;
    IRRef ofsref = 0;
    /* Handle constant offsets. */
    if (ir->o == IR_ADD && irref_isk(ir->op2)) {
      ofsref = ir->op2;
      ofs = IR(ofsref)->i;
      ref = ir->op1;
      ir = IR(ref);
    }
    /* Got scalar evolution analysis results for this reference? */
    if (ref == J->scev.idx) {
      int32_t stop;
      lua_assert(irt_isint(J->scev.t) && ir->o == IR_SLOAD);
      stop = lj_num2int(numV(&(J->L->base - J->baseslot)[ir->op1 + FORL_STOP]));
      /* Runtime value for stop of loop is within bounds? */
      if ((uint64_t)stop + ofs < (uint64_t)asize) {
        /* Emit invariant bounds check for stop. */
        emitir(IRTG(IR_ABC, IRT_P32), asizeref, ofs == 0 ? J->scev.stop :
               emitir(IRTI(IR_ADD), J->scev.stop, ofsref));
        /* Emit invariant bounds check for start, if not const or negative. */
        if (!(J->scev.dir && J->scev.start &&
              (int64_t)IR(J->scev.start)->i + ofs >= 0))
          emitir(IRTG(IR_ABC, IRT_P32), asizeref, ikey);
        return;
      }
    }
  }
  emitir(IRTGI(IR_ABC), asizeref, ikey);  /* Emit regular bounds check. */
}

/* -- Upvalue access ------------------------------------------------------ */

/* Check whether upvalue is immutable and ok to constify. */
static int rec_upvalue_constify(jit_State *J, GCupval *uvp)
{
  if (uvp->immutable) {
    const TValue *o = uvval(uvp);
    /* Don't constify objects that may retain large amounts of memory. */
#if LJ_HASFFI
    if (tviscdata(o)) {
      GCcdata *cd = cdataV(o);
      if (!cdataisv(cd) && !(cd->marked & LJ_GC_CDATA_FIN)) {
        CType *ct = ctype_raw(ctype_ctsG(J2G(J)), cd->ctypeid);
        if (!ctype_hassize(ct->info) || ct->size <= 16)
          return 1;
      }
      return 0;
    }
#else
    UNUSED(J);
#endif
    if (!(tvistab(o) || tvisudata(o) || tvisthread(o)))
      return 1;
  }
  return 0;
}

/* Record upvalue load/store. */
static TRef rec_upvalue(jit_State *J, uint32_t uv, TRef val)
{
  GCupval *uvp = J->fn->l.uvptr[uv];
  TRef fn = getcurrf(J);
  IRRef uref;
  int needbarrier = 0;
  if (rec_upvalue_constify(J, uvp)) {  /* Try to constify immutable upvalue. */
    TRef tr, kfunc;
    lua_assert(val == 0);
    if (!tref_isk(fn)) {  /* Late specialization of current function. */
      if (J->pt->flags >= PROTO_CLC_POLY)
        goto noconstify;
      kfunc = lj_ir_kfunc(J, J->fn);
      emitir(IRTG(IR_EQ, IRT_FUNC), fn, kfunc);
      J->base[-1] = TREF_FRAME | kfunc;
      fn = kfunc;
    }
    tr = lj_record_constify(J, uvval(uvp));
    if (tr)
      return tr;
  }
noconstify:
  /* Note: this effectively limits LJ_MAX_UPVAL to 127. */
  uv = (uv << 8) | (hashrot(uvp->dhash, uvp->dhash + HASH_BIAS) & 0xff);
  if (!uvp->closed) {
    uref = tref_ref(emitir(IRTG(IR_UREFO, IRT_P32), fn, uv));
    /* In current stack? */
    if (uvval(uvp) >= J->L->stack &&
        uvval(uvp) < J->L->maxstack) {
      int32_t slot = (int32_t)(uvval(uvp) - (J->L->base - J->baseslot));
      if (slot >= 0) {  /* Aliases an SSA slot? */
        emitir(IRTG(IR_EQ, IRT_P32),
               REF_BASE,
               emitir(IRT(IR_ADD, IRT_P32), uref,
                      lj_ir_kint(J, (slot - 1) * -8)));
        slot -= (int32_t)J->baseslot;  /* Note: slot number may be negative! */
        if (val == 0) {
          return rec_getslot(J, slot);
        } else {
          J->base[slot] = val;
          if (slot >= (int32_t)J->maxslot) J->maxslot = (BCReg)(slot+1);
          return 0;
        }
      }
    }
    emitir(IRTG(IR_UGT, IRT_P32),
          emitir(IRT(IR_SUB, IRT_P32), uref, REF_BASE),
          lj_ir_kint(J, (J->baseslot + J->maxslot) * 8));
  } else {
    needbarrier = 1;
    uref = tref_ref(emitir(IRTG(IR_UREFC, IRT_P32), fn, uv));
  }
  if (val == 0) {  /* Upvalue load */
    IRType t = itype2irt(uvval(uvp));
    TRef res = emitir(IRTG(IR_ULOAD, t), uref, 0);
    if (irtype_ispri(t)) res = TREF_PRI(t);  /* Canonicalize primitive refs. */
    return res;
  } else {  /* Upvalue store. */
    /* Convert int to number before storing. */
    if (tref_isinteger(val))
      val = emitir(IRTN(IR_CONV), val, IRCONV_NUM_INT);
    emitir(IRT(IR_USTORE, tref_type(val)), uref, val);
    if (needbarrier && tref_isgcv(val))
      emitir(IRT(IR_OBAR, IRT_NIL), uref, val);
    J->needsnap = 1;
    return 0;
  }
}

/* -- Record calls to Lua functions --------------------------------------- */

/* Check unroll limits for calls. */
static void check_call_unroll(jit_State *J, TraceNo lnk)
{
  const TValue *frame = J->L->base - 1;
  void *pc = (void*)(frame_func(frame)->l.pc);
  int32_t depth = J->framedepth;
  int32_t count = 0;
  if ((J->pt->flags & PROTO_VARARG)) depth--;  /* Vararg frame still missing. */
  for (; depth > 0; depth--) {  /* Count frames with same prototype. */
    if (frame_iscont(frame)) depth--;
    frame = frame_prev(frame);
    if ((void*)(frame_func(frame)->l.pc) == pc)
      count++;
  }
  if (J->pc == J->startpc) {
    if (count + J->tailcalled > J->param[JIT_P_recunroll]) {
      J->pc++;
      if (J->framedepth + J->retdepth == 0)
        rec_stop(J, LJ_TRLINK_TAILREC, J->cur.traceno);  /* Tail-recursion. */
      else
        rec_stop(J, LJ_TRLINK_UPREC, J->cur.traceno);  /* Up-recursion. */
    }
  } else {
    if (count > J->param[JIT_P_callunroll]) {
      if (lnk) {  /* Possible tail- or up-recursion. */
        lj_trace_flush(J, lnk);  /* Flush trace that only returns. */
        /* Set a small, pseudo-random hotcount for a quick retry of JFUNC*. */
        uj_hotcnt_set_counter((BCIns *)J->pc, LJ_PRNG_BITS(J, 4));
      }
      lj_trace_err(J, LJ_TRERR_CUNROLL);
    }
  }
}

/* Record Lua function setup. */
static void rec_func_setup(jit_State *J)
{
  const GCproto *pt = J->pt;
  const BCReg numparams = pt->numparams;
  BCReg s;
  if (uj_proto_jit_disabled(pt))
    lj_trace_err(J, LJ_TRERR_CJITOFF);
  if (J->baseslot + pt->framesize >= LJ_MAX_JSLOTS)
    lj_trace_err(J, LJ_TRERR_STACKOV);
  /* Fill up missing parameters with nil. */
  for (s = J->maxslot; s < numparams; s++)
    J->base[s] = TREF_NIL;
  /* The remaining slots should never be read before they are written. */
  J->maxslot = numparams;
}

/* Record Lua vararg function setup. */
static void rec_func_vararg(jit_State *J)
{
  GCproto *pt = J->pt;
  BCReg s, fixargs, vframe = J->maxslot+1;
  lua_assert((pt->flags & PROTO_VARARG));
  if (J->baseslot + vframe + pt->framesize >= LJ_MAX_JSLOTS)
    lj_trace_err(J, LJ_TRERR_STACKOV);
  J->base[vframe-1] = J->base[-1];  /* Copy function up. */
  /* Copy fixarg slots up and set their original slots to nil. */
  fixargs = pt->numparams < J->maxslot ? pt->numparams : J->maxslot;
  for (s = 0; s < fixargs; s++) {
    J->base[vframe+s] = J->base[s];
    J->base[s] = TREF_NIL;
  }
  J->maxslot = fixargs;
  J->framedepth++;
  J->base += vframe;
  J->baseslot += vframe;
}

/* Record entry to a Lua function. */
static void rec_func_lua(jit_State *J)
{
  rec_func_setup(J);
  check_call_unroll(J, 0);
}

/* Record entry to an already compiled function. */
static void rec_func_jit(jit_State *J, TraceNo lnk)
{
  GCtrace *T;
  rec_func_setup(J);
  T = traceref(J, lnk);
  if (T->linktype == LJ_TRLINK_RETURN) {  /* Trace returns to interpreter? */
    check_call_unroll(J, lnk);
    /* Temporarily unpatch JFUNC* to continue recording across function. */
    J->patchins = *J->pc;
    J->patchpc = (BCIns *)J->pc;
    *J->patchpc = T->startins;
    return;
  }
  J->instunroll = 0;  /* Cannot continue across a compiled function. */
  if (J->pc == J->startpc && J->framedepth + J->retdepth == 0)
    rec_stop(J, LJ_TRLINK_TAILREC, J->cur.traceno);  /* Extra tail-recursion. */
  else
    rec_stop(J, LJ_TRLINK_ROOT, lnk);  /* Link to the function. */
}

/* -- Vararg handling ----------------------------------------------------- */

/* Records loads from the vararg frame defined on-trace. */
static void rec_varg_ontrace(jit_State *J, BCReg dst, ptrdiff_t nresults) {
  int32_t numparams = J->pt->numparams;
  ptrdiff_t nvararg = frame_delta(J->L->base-1) - numparams - 1;

  ptrdiff_t i;
  if (nvararg < 0) {
    nvararg = 0;
  }

  if (nresults == -1) {
    nresults = nvararg;
    J->maxslot = dst + (BCReg)nvararg;
  } else if (dst + (BCReg)nresults > J->maxslot) {
    J->maxslot = dst + (BCReg)nresults;
  }

  for (i = 0; i < nresults; i++) {
    J->base[dst + i] = i < nvararg ? rec_getslot(J, (int32_t)(i - nvararg - 1)) : TREF_NIL;
  }
}

/* Aux structures and interfaces for recording BC_VARG.
**
** Layout for a vararg frame looks like this (from lower to higher addresses):
**
** [   nil   ]  <
** [   nil   ]  < dummy slots (always nil); number of dummy slots is equal to
** [   ...   ]  < the number of prototype's fixed arguments
** [   nil   ]  <
** [  varg1  ]  >
** [   ...   ]  > actual vararg slots
** [  vargN  ]  >
** [framelink]  < frame link of type FRAME_VARG
**
** Notes.
**  1. Dummy slots and the frame link are referred as aux slots.
**  2. Frame link's delta stores the number of aux slots +
**     the number of actual vararg slots.
**  3. frpad (see below) is a sum of the size of aux slots (in bytes)
**     and the frame type constant. This synthetic value is used for
**     caluclating effective number of varargs, effective address of certain
**     varargs etc. Frame type constant fixes up pointer alignment because
**     pointer arithmetic is done using vararg's *raw* ftsz which already
**     encapsulates the same constant.
*/

typedef struct rec_varg_context_t {
  const BCReg     dst;      /* BC_VARG's destination. */
  const ptrdiff_t nresults; /* Expected number of local slots filled
                            ** from the vararg frame. */
  const TRef      trftsz;   /* Runtime raw ftsz value of the vararg frame. */
  const int32_t   nparams;  /* Number of fixed parameters of the prototype. */
  const ptrdiff_t nvararg;  /* Record-time number of actual vararg slots. */
  const int32_t   frpad;    /* Padding+Fixup for vararg frame (see below). */
} rec_varg_context_t;

/* Initializes and returns aux recording context. */
static const rec_varg_context_t rec_varg_setup(const jit_State *J, BCReg dst, ptrdiff_t nresults, TRef trftsz) {
  const int32_t   nparams   = (int32_t)J->pt->numparams;
  const int32_t   nauxslots = nparams + 1;
  const ptrdiff_t nvararg   = frame_delta(J->L->base - 1) - nauxslots;

  lua_assert(nvararg >= 0);

  const rec_varg_context_t ctx = {
    .dst      = dst,
    .nresults = nresults,
    .trftsz   = trftsz,
    .nparams  = nparams,
    .nvararg  = nvararg,
    .frpad    = nauxslots * sizeof(TValue) + FRAME_VARG
  };

  return ctx;
}

/* Returns effective offset in bytes (fixup included) for the idx-th vararg. */
static LJ_AINLINE int32_t varg_slot_offset(const rec_varg_context_t *ctx, int32_t idx) {
  return ctx->frpad + idx * sizeof(TValue);
}

/* Emits IR for loading effective base of the vararg slot:
**  1. Subtract vararg's ftsz from current frame's base and get the address
**     of the raw vararg frame (including preceding nil slots for fixed
**     arguments).
**  2. Add frpad to position to address where actual varargs start.
** NB! ftsz and frpad are misaligned, but compensate each other. Besides, frpad
** encapsulates extra slot size (vararg frame's framelink). extra_bias is 0 in
** most cases, but it is used in case of recording select(numeric_literal, ...)
** to fixup 1-based indexing in Lua.
*/
static LJ_AINLINE TRef emit_vbase(jit_State *J, const rec_varg_context_t *ctx, int32_t extra_bias) {
  const int32_t bias = (1 + extra_bias) * sizeof(TValue);

  TRef vbase;

  vbase = emitir(IRT(IR_SUB, IRT_P32), REF_BASE, ctx->trftsz);
  vbase = emitir(IRT(IR_ADD, IRT_P32),    vbase,
    lj_ir_kint(J, ctx->frpad - bias));

  return vbase;
}

/* Emits load from the vararg slot given that vbase is already emitted.
** If record-time value is a primitive, it is canonicalized.
*/
static LJ_AINLINE TRef emit_vload(jit_State *J, const rec_varg_context_t *ctx, TRef vbase, TRef tridx, int32_t idx) {
  IRType type = itype2irt(&J->L->base[idx - (1 + ctx->nvararg)]);
  TRef aref   = emitir(IRT(IR_AREF, IRT_P32), vbase, tridx);
  TRef trres  = emitir(IRTG(IR_VLOAD, type), aref, 0);

  if (irtype_ispri(type)) { /* Canonicalize primitives. */
    trres = TREF_PRI(type);
  }

  return trres;
}

/* Aborts BC_VARG recording. */
static void rec_varg_abort(jit_State *J) {
  lj_trace_err_info_op(J, LJ_TRERR_NYIBC, (int32_t)BC_VARG);
}

/* Records copying fixed number of vararg slots to local slots, i.e. things like
**  local a, b, c = ... -- 3 slots should be handled here
** The basic scheme is:
**  1. Emit a guard for vararg frame size
**  2. If applicable, emit load of n elements from the vararg frame to stack
**  3. If actual number of vararg slots is less than required number of loads
*      fill the rest slots with nil
*/
static void rec_varg_fixed(jit_State *J, const rec_varg_context_t *ctx) {
  lua_assert(ctx->nresults >= 0);
  ptrdiff_t i;

  if (ctx->nvararg > 0) {
    /* At the time of recording, vararg frame contains varargs. */
    int has_enough_slots = ctx->nvararg >= ctx->nresults;
    ptrdiff_t nload      = has_enough_slots ? ctx->nresults : ctx->nvararg;

    if (has_enough_slots) {
      emitir(IRTGI(IR_GE), ctx->trftsz, lj_ir_kint(J, varg_slot_offset(ctx, ctx->nresults)));
    } else {
      emitir(IRTGI(IR_EQ), ctx->trftsz, lj_ir_kint(J, frame_ftsz(J->L->base - 1)));
    }

    TRef vbase = emit_vbase(J, ctx, 0);
    for (i = 0; i < nload; i++) {
      TRef tridx = lj_ir_kint(J, (int32_t)i);
      TRef trres = emit_vload(J, ctx, vbase, tridx, i);
      J->base[ctx->dst + i] = trres;
    }
  } else if (ctx->nvararg == 0) {
    /* At the time of recording, vararg frame does not contain varargs. */
    emitir(IRTGI(IR_LE), ctx->trftsz, lj_ir_kint(J, ctx->frpad));
  } else {
    lua_assert(0);
  }

  for (i = ctx->nvararg; i < ctx->nresults; i++) {
    J->base[ctx->dst + i] = TREF_NIL;
  }

  if (ctx->dst + (BCReg)ctx->nresults > J->maxslot) {
    J->maxslot = ctx->dst + (BCReg)ctx->nresults;
  }
}

/* Records the select(n, ...) idiom with various forms of n:
**  * literal/variable
**  * fits/does not fit into the vararg frame
** The basic scheme is:
**  1. Emit a guard to assert n against vararg frame size
**  2. If applicable, emit load of the n-th element of the vararg frame
**  3. Skip recording of subsequent bytecodes that implement call to select()
*/
static void rec_varg_select(jit_State *J, const rec_varg_context_t *ctx) {
  TRef tridx    = J->base[ctx->dst - 1];
  ptrdiff_t idx = lj_ffrecord_select_mode(J, tridx, &J->L->base[ctx->dst - 1]);
  if (idx < 0) {
    rec_varg_abort(J);
    return;
  }

  int select_nvararg  = idx == 0; /* x = select("#", ...) */
  int select_arg      = idx != 0; /* x = select( n , ...) */
  int select_in_frame = idx <= ctx->nvararg; /* meaningless for select("#", ...) */

  int32_t extra_bias = 0;

  /* Result in the form of typed reference. Default is a reference to nil. */
  TRef trres = TREF_NIL;

  if (select_arg && !tref_isinteger(tridx)) {
    tridx = emitir(IRTGI(IR_CONV), tridx, IRCONV_INT_NUM|IRCONV_INDEX);
  }

  if (select_arg && tref_isk(tridx)) {
    /* Record the call in the form
    **   select(numeric_literal, ...)
    */

    TRef troffset = lj_ir_kint(J, varg_slot_offset(ctx, (int32_t)idx));

    if (select_in_frame) {
      emitir(IRTGI(IR_GE), ctx->trftsz, troffset);
    } else {
      emitir(IRTGI(IR_LT), ctx->trftsz, troffset);
    }

    extra_bias = 1; /* Bias for 1-based index. */
  } else if (select_nvararg || (select_arg && select_in_frame)) {
    /* Record the call in the form
    **   select(n, ...)
    ** At the time of recording, n is either "#" (literal or variable) or a
    ** numeric variable pointing inside the vararg frame. In the "#" case,
    ** we emit only IR for calculating actual number of varargs. In the
    ** numeric case, we emit code for loading the single item on stack as well
    ** (see below).
    */

    TRef trszvararg = emitir(IRTI(IR_ADD), ctx->trftsz, lj_ir_kint(J, -ctx->frpad));
    if (ctx->nparams > 0) {
      emitir(IRTGI(IR_GE), trszvararg, lj_ir_kint(J, 0));
    }
    trres = emitir(IRTI(IR_BSHR), trszvararg, lj_ir_kint(J, LOG_SIZEOF_TVALUE));

    if (select_arg && select_in_frame) {
      tridx = emitir(IRTI(IR_ADD), tridx, lj_ir_kint(J, -1));
      lj_record_idx_abc(J, trres, tridx, (uint32_t)ctx->nvararg);
    }
  } else if (select_arg && !select_in_frame) {
    /* Record the call in the form
    **   select(n, ...)
    ** At the time of recording, n is the numeric variable pointing outside
    ** the vararg frame. We only emit a guard for this case for run-time checks.
    */
    TRef trfrpad  = lj_ir_kint(J, ctx->frpad);
    TRef troffset = emitir(IRTI(IR_BSHL), tridx   , lj_ir_kint(J, LOG_SIZEOF_TVALUE));
    trfrpad       = emitir(IRTI(IR_ADD) , troffset, trfrpad);

    emitir(IRTGI(IR_LT), ctx->trftsz, trfrpad);
  } else {
    lua_assert(0);
  }

  if (select_arg && select_in_frame) {
    TRef vbase = emit_vbase(J, ctx, extra_bias);
    trres      = emit_vload(J, ctx, vbase, tridx, idx - 1);
  }

  /* Skip CALLM + select. */
  J->base[ctx->dst - 2] = trres;
  J->maxslot = ctx->dst - 1;
  J->bcskip  = 2;
}

/* Detect y = select(x, ...) idiom. */
static int select_detect(jit_State *J) {
  BCIns ins = J->pc[1];
  if (!(bc_op(ins) == BC_CALLM && bc_b(ins) == 2 && bc_c(ins) == 1)) {
    return 0;
  }

  const TValue *func = &J->L->base[bc_a(ins)];
  if (!(tvisfunc(func) && funcV(func)->c.ffid == FF_select)) {
    return 0;
  }

  TRef kfunc = lj_ir_kfunc(J, funcV(func));
  emitir(IRTG(IR_EQ, IRT_FUNC), rec_getslot(J, (int32_t)bc_a(ins)), kfunc);
  return 1;
}

/* Record vararg instruction. */
static void rec_varg(jit_State *J, BCReg dst, ptrdiff_t nresults) {
  lua_assert(frame_isvarg(J->L->base - 1));

  if (J->framedepth > 0) { /* Simple case: varargs defined on-trace. */
    rec_varg_ontrace(J, dst, nresults);
    return;
  }

  /* Unknown number of varargs passed to trace. */

  TRef trftsz = emitir(IRTI(IR_SLOAD), 0, IRSLOAD_READONLY|IRSLOAD_FRAME);

  const rec_varg_context_t ctx = rec_varg_setup(J, dst, nresults, trftsz);

  if (nresults >= 0) {  /* Known fixed number of results. */
    rec_varg_fixed(J, &ctx);
  } else if (select_detect(J)) {  /* y = select(x, ...) */
    rec_varg_select(J, &ctx);
  } else {
    rec_varg_abort(J);
  }
}

/* -- Record allocations -------------------------------------------------- */

static TRef rec_tnew(jit_State *J, uint32_t ah)
{
  uint32_t asize = ah & 0x7ff;
  uint32_t hbits = ah >> 11;
  if (asize == 0x7ff) asize = 0x801;
  return emitir(IRTG(IR_TNEW, IRT_TAB), asize, hbits);
}

/* -- Concatenation ------------------------------------------------------- */

static TRef rec_cat(jit_State *J, BCReg baseslot, BCReg topslot) {
  BCReg slot;
  TRef trbuf, hdr;

  if (!(J->flags & JIT_F_OPT_JITCAT)) {
    lj_trace_err(J, LJ_TRERR_CATJITOFF);
  }

  lua_assert(baseslot < topslot);

  trbuf = hdr = emitir(IRT(IR_BUFHDR, IRT_PTR), lj_ir_kptr(J, &J2G(J)->tmpbuf),
                       IRBUFHDR_RESET);

  for (slot = baseslot; slot <= topslot; slot++) {
    TRef tr = rec_getslot(J, (int32_t)slot);
    if (tref_isnumber(tr))
      tr = emitir(IRT(IR_TOSTR, IRT_STR), tr, 0);
    if (!tref_isstr(tr))
      lj_trace_err(J, LJ_TRERR_CATBADTYPE);
    trbuf = emitir(IRT(IR_BUFPUT, IRT_PTR), trbuf, tr);
  }

  J->maxslot = baseslot;
  return emitir(IRT(IR_BUFSTR, IRT_STR), trbuf, hdr);
}

/* -- Record bytecode ops ------------------------------------------------- */

/* Prepare for comparison. */
static void rec_comp_prep(jit_State *J)
{
  /* Prevent merging with snapshot #0 (GC exit) since we fixup the PC. */
  if (J->cur.nsnap == 1 && J->cur.snap[0].ref == J->cur.nins)
    emitir_raw(IRT(IR_NOP, IRT_NIL), 0, 0);
  lj_snap_add(J);
}

/* Fixup comparison. */
static void rec_comp_fixup(jit_State *J, const BCIns *pc, int cond)
{
  BCIns jmpins = pc[1];
  const BCIns *npc = pc + 2 + (cond ? bc_j(jmpins) : 0);
  SnapShot *snap = &J->cur.snap[J->cur.nsnap-1];
  /* Set PC to opposite target to avoid re-recording the comp. in side trace. */
  snap_store_pc(&J->cur.snapmap[snap->mapofs + snap->nent], npc);
  J->needsnap = 1;
  if (bc_a(jmpins) < J->maxslot) J->maxslot = bc_a(jmpins);
  lj_snap_shrink(J);  /* Shrink last snapshot if possible. */
}

/* Record the next bytecode instruction (_before_ it's executed). */
void lj_record_ins(jit_State *J)
{
  const TValue *lbase;
  RecordIndex ix;
  const BCIns *pc;
  BCIns ins;
  BCOp op;
  TRef ra, rb, rc;

  /* Perform post-processing action before recording the next instruction. */
  if (LJ_UNLIKELY(J->postproc != LJ_POST_NONE)) {
    switch (J->postproc) {
    case LJ_POST_FIXCOMP:  /* Fixup comparison. */
      pc = frame_pc(&J2G(J)->tmptv);
      rec_comp_fixup(J, pc, (!tvistruecond(&J2G(J)->tmptv2) ^ (bc_op(*pc)&1)));
      /* fallthrough */
    case LJ_POST_FIXGUARD:  /* Fixup and emit pending guard. */
    case LJ_POST_FIXGUARDSNAP:  /* Fixup and emit pending guard and snapshot. */
      if (!tvistruecond(&J2G(J)->tmptv2)) {
        J->fold.ins.o ^= 1;  /* Flip guard to opposite. */
        if (J->postproc == LJ_POST_FIXGUARDSNAP) {
          SnapShot *snap = &J->cur.snap[J->cur.nsnap-1];
          J->cur.snapmap[snap->mapofs+snap->nent-1]--;  /* False -> true. */
        }
      }
      lj_opt_fold(J);  /* Emit pending guard. */
      /* fallthrough */
    case LJ_POST_FIXBOOL:
      if (!tvistruecond(&J2G(J)->tmptv2)) {
        BCReg s;
        TValue *tv = J->L->base;
        for (s = 0; s < J->maxslot; s++)  /* Fixup stack slot (if any). */
          if (J->base[s] == TREF_TRUE && tvisfalse(&tv[s])) {
            J->base[s] = TREF_FALSE;
            break;
          }
      }
      break;
    case LJ_POST_FIXCONST:
      {
        BCReg s;
        TValue *tv = J->L->base;
        for (s = 0; s < J->maxslot; s++)  /* Constify stack slots (if any). */
          if (J->base[s] == TREF_NIL && !tvisnil(&tv[s]))
            J->base[s] = lj_record_constify(J, &tv[s]);
      }
      break;
    case LJ_POST_FFSPECRET:
      {
        /*
         * Fix-up for fast functions that do not have a "fixed" return type.
         * By convention, any of returned values set to NULL is a signal for a
         * side exit. Current implementation handles only cases when exactly one
         * value is returned -- generalize on demand.
         */
        BCIns ins = *(J->pc - 1);
        BCReg s;
        const TValue *tv = J->L->base;

        if (!tvistruecond(&J2G(J)->tmptv2))
          lj_trace_err(J, LJ_TRERR_GFAIL);

        lua_assert(bc_op(ins) == BC_CALL ||
                   bc_op(ins) == BC_CALLT ||
                   bc_op(ins) == BC_CALLM ||
                   bc_op(ins) == BC_CALLMT);

        s = bc_a(ins);
        emitir(IRTG(IR_NE, IRT_PTR), J->base[s], lj_ir_kkptr(J, NULL));
        J->base[s] = emitir(IRTG(IR_TVLOAD, itype2irt(&tv[s])), J->base[s], 0);
      }
      /* fallthrough */
    case LJ_POST_FFRETRY:  /* Suppress recording of retried fast function. */
      if (bc_op(*J->pc) >= BC__MAX)
        return;
      break;
    default: lua_assert(0); break;
    }
    J->postproc = LJ_POST_NONE;
  }

  /* Need snapshot before recording next bytecode (e.g. after a store). */
  if (J->needsnap) {
    J->needsnap = 0;
    lj_snap_purge(J);
    lj_snap_add(J);
    J->mergesnap = 1;
  }

  /* Skip some bytecodes. */
  if (LJ_UNLIKELY(J->bcskip > 0)) {
    J->bcskip--;
    return;
  }

  /* Record only closed loops for root traces. */
  pc = J->pc;
  if (J->framedepth == 0 &&
     (size_t)((char *)pc - (char *)J->bc_min) >= J->bc_extent)
    lj_trace_err(J, LJ_TRERR_LLEAVE);

#ifndef NDEBUG
  rec_check_slots(J);
  rec_check_ir(J);
#endif

  /* Keep a copy of the runtime values of var/num/str operands. */
#define rav     (&ix.valv)
#define rbv     (&ix.tabv)
#define rcv     (&ix.keyv)

  lbase = J->L->base;
  ins = *pc;
  op = bc_op(ins);
  ra = bc_a(ins);
  ix.val = 0;
  switch (bcmode_a(op)) {
  case BCMvar:
    copyTV(J->L, rav, &lbase[ra]); ix.val = ra = rec_getslot(J, (int32_t)ra); break;
  default: break;  /* Handled later. */
  }
  rb = bc_b(ins);
  rc = bc_c(ins);
  switch (bcmode_b(op)) {
  case BCMnone: rb = 0; rc = bc_d(ins); break;  /* Upgrade rc to 'rd'. */
  case BCMvar:
    copyTV(J->L, rbv, &lbase[rb]); ix.tab = rb = rec_getslot(J, (int32_t)rb); break;
  default: break;  /* Handled later. */
  }
  switch (bcmode_c(op)) {
  case BCMvar:
    copyTV(J->L, rcv, &lbase[rc]); ix.key = rc = rec_getslot(J, (int32_t)rc); break;
  case BCMpri: settag(rcv, ~rc); ix.key = rc = TREF_PRI(IRT_NIL+rc); break;
  case BCMnum: { const TValue *tv = proto_knumtv(J->pt, rc);
    copyTV(J->L, rcv, tv); ix.key = rc = lj_ir_knumint(J, numV(tv)); } break;
  case BCMstr: { GCstr *s = gco2str(proto_kgc(J->pt, ~(ptrdiff_t)rc));
    setstrV(J->L, rcv, s); ix.key = rc = lj_ir_kstr(J, s); } break;
  default: break;  /* Handled later. */
  }

  switch (op) {

  /* -- Comparison ops ---------------------------------------------------- */

  case BC_ISLT: case BC_ISGE: case BC_ISLE: case BC_ISGT:
#if LJ_HASFFI
    if (tref_iscdata(ra) || tref_iscdata(rc)) {
      rec_mm_comp_cdata(J, &ix, op, ((int)op & 2) ? MM_le : MM_lt);
      break;
    }
#endif
    /* Emit nothing for two numeric or string consts. */
    if (!(tref_isk2(ra,rc) && tref_isnumber_str(ra) && tref_isnumber_str(rc))) {
      IRType ta = tref_isinteger(ra) ? IRT_INT : tref_type(ra);
      IRType tc = tref_isinteger(rc) ? IRT_INT : tref_type(rc);
      int irop;
      if (ta != tc) {
        /* Widen mixed number/int comparisons to number/number comparison. */
        if (ta == IRT_INT && tc == IRT_NUM) {
          ra = emitir(IRTN(IR_CONV), ra, IRCONV_NUM_INT);
          ta = IRT_NUM;
        } else if (ta == IRT_NUM && tc == IRT_INT) {
          rc = emitir(IRTN(IR_CONV), rc, IRCONV_NUM_INT);
        } else if (LJ_52) {
          ta = IRT_NIL;  /* Force metamethod for different types. */
        } else if (!((ta == IRT_FALSE || ta == IRT_TRUE) &&
                     (tc == IRT_FALSE || tc == IRT_TRUE))) {
          break;  /* Interpreter will throw for two different types. */
        }
      }
      rec_comp_prep(J);
      irop = (int)op - (int)BC_ISLT + (int)IR_LT;
      if (ta == IRT_NUM) {
        if ((irop & 1)) irop ^= 4;  /* ISGE/ISGT are unordered. */
        if (!lj_ir_numcmp(numV(rav), numV(rcv), (IROp)irop))
          irop ^= 5;
      } else if (ta == IRT_INT) {
        if (!lj_ir_numcmp(numV(rav), numV(rcv), (IROp)irop))
          irop ^= 1;
      } else if (ta == IRT_STR) {
        if (!lj_ir_strcmp(strV(rav), strV(rcv), (IROp)irop)) irop ^= 1;
        ra = lj_ir_call(J, IRCALL_uj_str_cmp, ra, rc);
        rc = lj_ir_kint(J, 0);
        ta = IRT_INT;
      } else {
        rec_mm_comp(J, &ix, (int)op);
        break;
      }
      emitir(IRTG(irop, ta), ra, rc);
      rec_comp_fixup(J, J->pc, ((int)op ^ irop) & 1);
    }
    break;

  case BC_ISEQV: case BC_ISNEV:
  case BC_ISEQS: case BC_ISNES:
  case BC_ISEQN: case BC_ISNEN:
  case BC_ISEQP: case BC_ISNEP:
#if LJ_HASFFI
    if (tref_iscdata(ra) || tref_iscdata(rc)) {
      rec_mm_comp_cdata(J, &ix, op, MM_eq);
      break;
    }
#endif
    /* Emit nothing for two non-table, non-udata consts. */
    if (!(tref_isk2(ra, rc) && !(tref_istab(ra) || tref_isudata(ra)))) {
      int diff;
      rec_comp_prep(J);
      diff = lj_record_objcmp(J, ra, rc, rav, rcv);
      if (diff == 2 || !(tref_istab(ra) || tref_isudata(ra)))
        rec_comp_fixup(J, J->pc, ((int)op & 1) == !diff);
      else if (diff == 1)  /* Only check __eq if different, but same type. */
        rec_mm_equal(J, &ix, (int)op);
    }
    break;

  /* -- Unary test and copy ops ------------------------------------------- */

  case BC_ISTC: case BC_ISFC:
    if ((op & 1) == tref_istruecond(rc))
      rc = 0;  /* Don't store if condition is not true. */
    /* fallthrough */
  case BC_IST: case BC_ISF:  /* Type specialization suffices. */
    if (bc_a(pc[1]) < J->maxslot)
      J->maxslot = bc_a(pc[1]);  /* Shrink used slots. */
    break;

  /* -- Unary ops --------------------------------------------------------- */

  case BC_NOT:
    /* Type specialization already forces const result. */
    rc = tref_istruecond(rc) ? TREF_FALSE : TREF_TRUE;
    break;

  case BC_LEN:
    if (tref_isstr(rc))
      rc = emitir(IRTI(IR_FLOAD), rc, IRFL_STR_LEN);
    else if (!LJ_52 && tref_istab(rc))
      rc = lj_ir_call(J, IRCALL_lj_tab_len, rc);
    else
      rc = rec_mm_len(J, rc, rcv);
    break;

  /* -- Arithmetic ops ---------------------------------------------------- */

  case BC_UNM:
    if (tref_isnumber_str(rc)) {
      rc = lj_opt_narrow_unm(J, rc, rcv);
    } else {
      ix.tab = rc;
      copyTV(J->L, &ix.tabv, rcv);
      rc = rec_mm_arith(J, &ix, MM_unm);
    }
    break;

  case BC_ADD:
  case BC_SUB:
  case BC_MUL:
  case BC_DIV:
    {
    enum MMS mm = bcmode_mm(op);
    if (tref_isnumber_str(rb) && tref_isnumber_str(rc))
      rc = lj_opt_narrow_arith(J, rb, rc, rbv, rcv,
                               (int)mm - (int)MM_add + (int)IR_ADD);
    else
      rc = rec_mm_arith(J, &ix, mm);
    break;
    }

  case BC_MOD:
    if (tref_isnumber_str(rb) && tref_isnumber_str(rc))
      rc = lj_opt_narrow_mod(J, rb, rc, rbv, rcv);
    else
      rc = rec_mm_arith(J, &ix, MM_mod);
    break;

  case BC_POW:
    if (tref_isnumber_str(rb) && tref_isnumber_str(rc))
      rc = lj_opt_narrow_pow(J, rb, rc, rbv, rcv);
    else
      rc = rec_mm_arith(J, &ix, MM_pow);
    break;

  /* -- Miscellaneous ops ------------------------------------------------- */

  case BC_CAT:
    rc = rec_cat(J, rb, rc);
    break;

  /* -- Constant and move ops --------------------------------------------- */

  case BC_MOV:
    /* Clear gap of method call to avoid resurrecting previous refs. */
    if (ra > J->maxslot) J->base[ra-1] = 0;
    break;
  case BC_KSTR: case BC_KNUM: case BC_KPRI:
    break;
  case BC_KSHORT:
    rc = lj_ir_kint(J, (int32_t)(int16_t)rc);
    break;
  case BC_KNIL:
    while (ra <= rc)
      J->base[ra++] = TREF_NIL;
    if (rc >= J->maxslot) J->maxslot = rc+1;
    break;
#if LJ_HASFFI
  case BC_KCDATA:
    rc = lj_ir_kgc(J, proto_kgc(J->pt, ~(ptrdiff_t)rc), IRT_CDATA);
    break;
#endif

  /* -- Upvalue and function ops ------------------------------------------ */

  case BC_UGET:
    rc = rec_upvalue(J, rc, 0);
    break;
  case BC_USETV: case BC_USETS: case BC_USETN: case BC_USETP:
    rec_upvalue(J, ra, rc);
    break;

  /* -- Table ops --------------------------------------------------------- */

  case BC_GGET: case BC_GSET:
    settabV(J->L, &ix.tabv, J->fn->l.env);
    ix.tab = emitir(IRT(IR_FLOAD, IRT_TAB), getcurrf(J), IRFL_FUNC_ENV);
    ix.idxchain = LJ_MAX_IDXCHAIN;
    rc = uj_record_indexed(J, &ix);
    break;

  case BC_TGETB: case BC_TSETB:
    setintV(&ix.keyv, (int32_t)rc);
    ix.key = lj_ir_kint(J, (int32_t)rc);
    /* fallthrough */
  case BC_TGETV: case BC_TGETS: case BC_TSETV: case BC_TSETS:
    ix.idxchain = LJ_MAX_IDXCHAIN;
    rc = uj_record_indexed(J, &ix);
    break;

  case BC_TNEW:
    rc = rec_tnew(J, rc);
    break;
  case BC_TDUP:
    rc = emitir(IRTG(IR_TDUP, IRT_TAB),
                lj_ir_ktab(J, gco2tab(proto_kgc(J->pt, ~(ptrdiff_t)rc))), 0);
    break;

  /* -- Calls and vararg handling ----------------------------------------- */

  case BC_ITERC:
    J->base[ra] = rec_getslot(J, (int32_t)(ra - 3));
    J->base[ra + 1] = rec_getslot(J, (int32_t)(ra - 2));
    J->base[ra + 2] = rec_getslot(J, (int32_t)(ra - 1));
    { /* Do the actual copy now because lj_record_call needs the values. */
      TValue *b = &J->L->base[ra];
      copyTV(J->L, b, b-3);
      copyTV(J->L, b+1, b-2);
      copyTV(J->L, b+2, b-1);
    }
    lj_record_call(J, ra, (ptrdiff_t)rc-1);
    break;

  case BC_ITERN:
    if (!(J->flags & JIT_F_OPT_JITPAIRS))
      lj_trace_err_info_op(J, LJ_TRERR_NYIBC, BC_ITERN);

    rec_itern(J, ra);
    break;

  /* L->top is set to L->base+ra+rc+NARGS-1+1. See uj_hook_ins(). */
  case BC_CALLM:
    rc = (BCReg)(J->L->top - J->L->base) - ra;
    /* fallthrough */
  case BC_CALL:
    lj_record_call(J, ra, (ptrdiff_t)rc-1);
    break;

  case BC_CALLMT:
    rc = (BCReg)(J->L->top - J->L->base) - ra;
    /* fallthrough */
  case BC_CALLT:
    lj_record_tailcall(J, ra, (ptrdiff_t)rc-1);
    break;

  case BC_VARG:
    rec_varg(J, ra, (ptrdiff_t)rb-1);
    break;

  /* -- Returns ----------------------------------------------------------- */

  case BC_RETM:
    /* L->top is set to L->base+ra+rc+NRESULTS-1, see uj_dispatch_ins(). */
    rc = (BCReg)(J->L->top - J->L->base) - ra + 1;
    /* fallthrough */
  case BC_RET: case BC_RET0: case BC_RET1:
    lj_record_ret(J, ra, (ptrdiff_t)rc-1);
    break;

  /* -- Loops and branches ------------------------------------------------ */

  case BC_FORI:
    if (rec_for(J, pc, 0) != LOOPEV_LEAVE)
      J->loopref = J->cur.nins;
    break;
  case BC_JFORI:
    lua_assert(bc_op(pc[(ptrdiff_t)rc-BCBIAS_J]) == BC_JFORL);
    if (rec_for(J, pc, 0) != LOOPEV_LEAVE)  /* Link to existing loop. */
      rec_stop(J, LJ_TRLINK_ROOT, bc_d(pc[(ptrdiff_t)rc-BCBIAS_J]));
    /* Continue tracing if the loop is not entered. */
    break;

  case BC_FORL:
    rec_loop_interp(J, pc, rec_for(J, pc+((ptrdiff_t)rc-BCBIAS_J), 1));
    break;
  case BC_ITERL:
  case BC_ITRNL:
    rec_loop_interp(J, pc, rec_iterl(J, *pc));
    break;
  case BC_LOOP:
    rec_loop_interp(J, pc, rec_loop(J, ra));
    break;

  case BC_JFORL:
    rec_loop_jit(J, rc, rec_for(J, pc+bc_j(traceref(J, rc)->startins), 1));
    break;
  case BC_JITERL:
  case BC_JITRNL:
    rec_loop_jit(J, rc, rec_iterl(J, traceref(J, rc)->startins));
    break;
  case BC_JLOOP:
    rec_loop_jit(J, rc, rec_loop(J, ra));
    break;

  case BC_IFORL:
  case BC_IITERL:
  case BC_IITRNL:
  case BC_ILOOP:
  case BC_IFUNCF:
  case BC_IFUNCV:
    lj_trace_err(J, LJ_TRERR_BLACKL);
    break;

  case BC_JMP:
    if (ra < J->maxslot)
      J->maxslot = ra;  /* Shrink used slots. */
    break;

  case BC_ISNEXT:
    if (!(J->flags & JIT_F_OPT_JITPAIRS))
      lj_trace_err_info_op(J, LJ_TRERR_NYIBC, BC_ISNEXT);

    if (ra < J->maxslot) /* Shrink used slots */
      J->maxslot = ra;
    rec_isnext(J, ra);
    break;

  /* -- Function headers -------------------------------------------------- */

  case BC_FUNCF:
    rec_func_lua(J);
    break;
  case BC_JFUNCF:
    rec_func_jit(J, rc);
    break;

  case BC_FUNCV:
    rec_func_vararg(J);
    rec_func_lua(J);
    break;
  case BC_JFUNCV:
    lua_assert(0);  /* Cannot happen. No hotcall counting for varag funcs. */
    break;

  case BC_FUNCC:
  case BC_FUNCCW:
    lj_ffrecord_func(J);
    break;
  case BC_HOTCNT:
  case BC_COVERG: /* Will not stream lineinfo from TRACE */
    break;

  default:
    if (op >= BC__MAX) {
      lj_ffrecord_func(J);
      break;
    }
    /* fallthrough */
  case BC_UCLO:
  case BC_FNEW:
  case BC_TSETM:
    lj_trace_err_info_op(J, LJ_TRERR_NYIBC, (int32_t)op);
    break;
  }

  /* rc == 0 if we have no result yet, e.g. pending __index metamethod call. */
  if (bcmode_a(op) == BCMdst && rc) {
    J->base[ra] = rc;
    if (ra >= J->maxslot) J->maxslot = ra+1;
  }

#undef rav
#undef rbv
#undef rcv

  /* Limit the number of recorded IR instructions. */
  if (J->cur.nins > REF_FIRST+(IRRef)J->param[JIT_P_maxrecord])
    lj_trace_err(J, LJ_TRERR_TRACEOV);
}

/* -- Recording setup ----------------------------------------------------- */

/* Setup recording for a root trace started by a hot loop. */
static const BCIns *rec_setup_root(jit_State *J)
{
  /* Determine the next PC and the bytecode range for the loop. */
  const BCIns *pcj, *pc = J->pc;
  BCIns ins = *pc;
  BCReg ra = bc_a(ins);
  switch (bc_op(ins)) {
  case BC_FORL:
    J->bc_extent = (size_t)(-bc_j(ins))*sizeof(BCIns);
    pc += 1+bc_j(ins);
    J->bc_min = pc;
    break;
  case BC_ITERL:
  case BC_ITRNL:
    lua_assert(bc_op(pc[-1]) == BC_ITERC || bc_op(pc[-1]) == BC_ITERN);
    J->maxslot = ra + bc_b(pc[-1]) - 1;
    J->bc_extent = (size_t)(-bc_j(ins))*sizeof(BCIns);
    pc += 1+bc_j(ins);
    lua_assert(bc_op(pc[-1]) == BC_JMP || bc_op(pc[-1]) == BC_ISNEXT);
    J->bc_min = pc;
    break;
  case BC_LOOP:
    /* Only check BC range for real loops, but not for "repeat until true". */
    pcj = pc + bc_j(ins);
    ins = *pcj;
    if (bc_op(ins) == BC_JMP && bc_j(ins) < 0) {
      J->bc_min = pcj+1 + bc_j(ins);
      J->bc_extent = (size_t)(-bc_j(ins))*sizeof(BCIns);
    }
    J->maxslot = ra;
    pc++;
    break;
  case BC_RET:
  case BC_RET0:
  case BC_RET1:
    /* No bytecode range check for down-recursive root traces. */
    J->maxslot = ra + bc_d(ins) - 1;
    break;
  case BC_FUNCF:
    /* No bytecode range check for root traces started by a hot call. */
    J->maxslot = J->pt->numparams;
    pc++;
    break;
  default:
    lua_assert(0);
    break;
  }
  return pc;
}

/* Setup for recording a new trace. */
void lj_record_setup(jit_State *J)
{
  uint32_t i;

  memset(&(J->abortstate), 0, sizeof(J->abortstate));

  /* Initialize state related to current trace. */
  memset(J->slot, 0, sizeof(J->slot));
  memset(J->chain, 0, sizeof(J->chain));
  memset(J->bpropcache, 0, sizeof(J->bpropcache));
  J->scev.idx = REF_NIL;
  J->scev.pc = NULL;

  J->baseslot = 1;  /* Invoking function is at base[-1]. */
  J->base = J->slot + J->baseslot;
  J->maxslot = 0;
  J->framedepth = 0;
  J->retdepth = 0;

  J->instunroll = J->param[JIT_P_instunroll];
  J->loopunroll = J->param[JIT_P_loopunroll];
  J->tailcalled = 0;
  J->loopref = 0;

  J->bc_min = NULL;  /* Means no limit. */
  J->bc_extent = ~(size_t)0;

  /* Emit instructions for fixed references. Also triggers initial IR alloc. */
  emitir_raw(IRT(IR_BASE, IRT_P32), J->parent, J->exitno);
  for (i = 0; i <= 2; i++) {
    IRIns *ir = IR(REF_NIL-i);
    ir->i = 0;
    ir->t.irt = (uint8_t)(IRT_NIL+i);
    ir->o = IR_KPRI;
    ir->prev = 0;
  }
  J->cur.nk = REF_TRUE;

  J->startpc = J->pc;
  J->cur.startpc = (BCIns*)J->pc;

  if (J->parent) {  /* Side trace. */
    GCtrace *T = traceref(J, J->parent);
    TraceNo root = T->root ? T->root : J->parent;
    J->cur.root = (uint16_t)root;
    J->cur.startins = BCINS_AD(BC_JMP, 0, 0);
    /* Check whether we could at least potentially form an extra loop. */
    if (J->exitno == 0 && T->snap[0].nent == 0) {
      /* We can narrow a FORL for some side traces, too. */
      if (J->pc > proto_bc(J->pt) && bc_op(J->pc[-1]) == BC_JFORI &&
          bc_d(J->pc[bc_j(J->pc[-1])-1]) == root) {
        lj_snap_add(J);
        rec_for_loop(J, J->pc-1, &J->scev, 1);
        goto sidecheck;
      }
    } else {
      J->startpc = NULL;  /* Prevent forming an extra loop. */
    }
    lj_snap_replay(J, T);
  sidecheck:
    if (traceref(J, J->cur.root)->nchild >= J->param[JIT_P_maxside] ||
        T->snap[J->exitno].count >= J->param[JIT_P_hotexit] +
                                    J->param[JIT_P_tryside]) {
      rec_stop(J, LJ_TRLINK_INTERP, 0);
    }
  } else {  /* Root trace. */
    J->cur.root = 0;
    J->cur.startins = *J->pc;
    J->pc = rec_setup_root(J);
    /* Note: the loop instruction itself is recorded at the end and not
    ** at the start! So snapshot #0 needs to point to the *next* instruction.
    */
    lj_snap_add(J);
    if (bc_op(J->cur.startins) == BC_FORL)
      rec_for_loop(J, J->pc-1, &J->scev, 1);
    if (1 + J->pt->framesize >= LJ_MAX_JSLOTS)
      lj_trace_err(J, LJ_TRERR_STACKOV);
  }
#ifdef LUAJIT_ENABLE_CHECKHOOK
  /* Regularly check for instruction/line hooks from compiled code and
  ** exit to the interpreter if the hooks are set.
  **
  ** This is a compile-time option and disabled by default, since the
  ** hook checks may be quite expensive in tight loops.
  **
  ** Note this is only useful if hooks are *not* set most of the time.
  ** Use this only if you want to *asynchronously* interrupt the execution.
  **
  ** You can set the instruction hook via lua_sethook() with a count of 1
  ** from a signal handler or another native thread. Please have a look
  ** at the first few functions in luajit.c for an example (Ctrl-C handler).
  */
  {
    TRef tr = emitir(IRT(IR_XLOAD, IRT_U8),
                     lj_ir_kptr(J, &J2G(J)->hookmask), IRXLOAD_VOLATILE);
    tr = emitir(IRTI(IR_BAND), tr, lj_ir_kint(J, (LUA_MASKLINE|LUA_MASKCOUNT)));
    emitir(IRTGI(IR_EQ), tr, lj_ir_kint(J, 0));
  }
#endif
}

#undef IR
#undef emitir_raw
#undef emitir

#endif
