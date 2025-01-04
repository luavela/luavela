/*
 * Trace management.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "lj_obj.h"

#if LJ_HASJIT

#include "lj_gc.h"
#include "uj_mem.h"
#include "uj_throw.h"
#include "lj_frame.h"
#include "lj_bc.h"
#include "uj_proto.h"
#include "uj_state.h"
#include "jit/lj_ir.h"
#include "jit/lj_jit.h"
#include "jit/lj_iropt.h"
#include "jit/lj_mcode.h"
#include "jit/lj_trace.h"
#include "jit/lj_snap.h"
#include "uj_gdbjit.h"
#include "uj_vtunejit.h"
#include "jit/lj_record.h"
#include "jit/lj_asm.h"
#include "uj_dispatch.h"
#include "uj_hook.h"
#include "lj_vm.h"
#include "uj_cframe.h"
#include "uj_hotcnt.h"
#include "jit/lj_target.h"
#include "dump/uj_dump_iface.h"

/* -- Error handling ------------------------------------------------------ */

/*
 * Trace aborts. There are two kinds of aborts:
 *  * Synchronous: JIT compiler performs a faulty operation and refuses to
 *    proceed. Examples: an attempt to record a NYI bytecode, hitting memory
 *    limits for machine code, etc.
 *  * Asynchronous: JIT's work is interrupted by another subsystem, and it is
 *    safe to anort current session and retry later. Examples: a hook is called
 *    during recording, a __gc finalizer is called during recording, etc.
 *
 * In case of asynchronous aborts, lj_trace_abort is called directly.
 * In case of synchronous aborts, it is called via lj_trace_err* interfaces.
 * The function populates the abort context which stores information about
 * failed coroutine, consider following example:
 *
 *  1. JIT is active; some byte code gets recorded successfully.
 *  2. During execution, a run-time error is thrown.
 *  3. This exception is propagated to an appropriate catch frame.
 *  4. Now assume we resume execution of another coroutine within
 *     the same Lua machine. Since JIT is active, resumption will result in
 *     entering JIT's event loop which goal in this context is to clean things
 *     up after the abort and transfer control to the interpreter.
 *  5. At this moment, we must guarantee that operations like stack unwinding,
 *     error object inspection et al. will be performing on the original
 *     failed coroutine. The abort context serves exactly this purpose.
 */
void lj_trace_abort(global_State *g) {
  jit_State *J = G2J(g);

  J->abortstate.L  = J->L;
  J->abortstate.pc = J->pc;

  J->state &= ~LJ_TRACE_ACTIVE;
}

/* Synchronous abort with error message. */
void lj_trace_err(jit_State *J, TraceError e)
{
  setnilV(&J->abortstate.extra_data); /* No extra data for the error message. */
  setintV(J->L->top++, (int32_t)e);
  uj_throw(J->L, LUA_ERRRUN);
}

/* Synchronous abort with error message and error info. */
LJ_NORET static void trace_err_info(jit_State *J, TraceError e) {
  setintV(J->L->top++, (int32_t)e);
  uj_throw(J->L, LUA_ERRRUN);
}

/* Synchronous abort with error message and traced function as an error info. */
void lj_trace_err_info_func(jit_State *J, TraceError e) {
  setfuncV(J->L, &(J->abortstate.extra_data), J->fn);
  trace_err_info(J, e);
}

/* Synchronous abort with error message and BC or IR op code as an error info. */
void lj_trace_err_info_op(jit_State *J, TraceError e, int32_t op) {
  setintV(&(J->abortstate.extra_data), op);
  trace_err_info(J, e);
}

/* -- Trace management ---------------------------------------------------- */

/* The current trace is first assembled in J->cur. The variable length
** arrays point to shared, growable buffers (J->irbuf etc.). When trace
** recording ends successfully, the current trace and its data structures
** are copied to a new (compact) GCtrace object.
*/

/* Find a free trace number. */
static TraceNo trace_findfree(jit_State *J)
{
  size_t osz, lim;
  if (J->freetrace == 0)
    J->freetrace = 1;
  for (; J->freetrace < J->sizetrace; J->freetrace++)
    if (traceref(J, J->freetrace) == NULL)
      return J->freetrace++;
  /* Need to grow trace array. */
  lim = (size_t)J->param[JIT_P_maxtrace] + 1;
  if (lim < 2) lim = 2; else if (lim > 65535) lim = 65535;
  osz = J->sizetrace;
  if (osz >= lim)
    return 0;  /* Too many traces. */
  J->trace = (GCtrace **)uj_mem_grow(J->L, J->trace,
                                     &(J->sizetrace), lim, sizeof(GCtrace *));
  for (; osz < J->sizetrace; osz++)
    J->trace[osz] = NULL;
  return J->freetrace;
}

#define TRACE_APPENDVEC(field, szfield, tp) \
  T->field = (tp *)p; \
  memcpy(p, J->cur.field, J->cur.szfield*sizeof(tp)); \
  p += J->cur.szfield*sizeof(tp);

/* Allocate space for copy of trace. */
static GCtrace *trace_save_alloc(jit_State *J)
{
  size_t sztr = ((sizeof(GCtrace)+7)&~7);
  size_t szins = (J->cur.nins-J->cur.nk)*sizeof(IRIns);
  size_t sz = sztr + szins +
              J->cur.nsnap*sizeof(SnapShot) +
              J->cur.nsnapmap*sizeof(SnapEntry);
  return uj_mem_alloc(J->L, sz);
}

/* Save current trace by copying and compacting it. */
static void trace_save(jit_State *J, GCtrace *T)
{
  size_t sztr = ((sizeof(GCtrace)+7)&~7);
  size_t szins = (J->cur.nins-J->cur.nk)*sizeof(IRIns);
  char *p = (char *)T + sztr;
  memcpy(T, &J->cur, sizeof(GCtrace));
  T->nextgc = J2G(J)->gc.root;
  J2G(J)->gc.root = obj2gco(T);
  newwhite(J2G(J), T);
  T->gct = ~LJ_TTRACE;
  T->ir = (IRIns *)p - J->cur.nk;
  memcpy(p, J->cur.ir+J->cur.nk, szins);
  p += szins;
  TRACE_APPENDVEC(snap, nsnap, SnapShot)
  TRACE_APPENDVEC(snapmap, nsnapmap, SnapEntry)
  J->cur.traceno = 0;
  J->trace[T->traceno] = T;
  lj_gc_barriertrace(J2G(J), T->traceno);
  uj_gdbjit_addtrace(J, T);
  uj_vtunejit_addtrace(J, T);
}

size_t lj_trace_sizeof(GCtrace *T) {
  return ((sizeof(GCtrace)+7)&~7) + (T->nins-T->nk)*sizeof(IRIns) +
         T->nsnap*sizeof(SnapShot) + T->nsnapmap*sizeof(SnapEntry);
}

void lj_trace_free(global_State *g, GCtrace *T)
{
  jit_State *J = G2J(g);
  if (T->traceno) {
    uj_gdbjit_deltrace(J, T);
    uj_vtunejit_deltrace(J, T);
    if (T->traceno < J->freetrace)
      J->freetrace = T->traceno;
    J->trace[T->traceno] = NULL;
  }
  uj_mem_free(MEM_G(g), T, lj_trace_sizeof(T));
}

/* Unpatch the bytecode modified by a root trace. */
static void trace_unpatch(jit_State *J, GCtrace *T)
{
  BCOp op = bc_op(T->startins);
  BCIns *pc = T->startpc;
  UNUSED(J);
  if (op == BC_JMP)
    return;  /* No need to unpatch branches in parent traces (yet). */
  switch (bc_op(*pc)) {
  case BC_JFORL:
    lua_assert(traceref(J, bc_d(*pc)) == T);
    *pc = T->startins;
    pc += bc_j(T->startins);
    lua_assert(bc_op(*pc) == BC_JFORI);
    setbc_op(pc, BC_FORI);
    break;
  case BC_JITERL:
  case BC_JITRNL:
  case BC_JLOOP:
    lua_assert(op == BC_ITERL || op == BC_ITRNL || op == BC_LOOP || bc_isret(op));
    *pc = T->startins;
    break;
  case BC_JMP:
    lua_assert(op == BC_ITERL);
    pc += bc_j(*pc)+2;
    if (bc_op(*pc) == BC_JITERL) {
      lua_assert(traceref(J, bc_d(*pc)) == T);
      *pc = T->startins;
    }
    break;
  case BC_JFUNCF:
    lua_assert(op == BC_FUNCF);
    *pc = T->startins;
    break;
  default:  /* Already unpatched. */
    break;
  }
}

/* Flush a root trace. */
static void trace_flushroot(jit_State *J, GCtrace *T)
{
  GCproto *pt = T->startpt;
  lua_assert(T->root == 0 && pt != NULL);
  /* First unpatch any modified bytecode. */
  trace_unpatch(J, T);
  /* Unlink root trace from chain anchored in prototype. */
  if (pt->trace == T->traceno) {  /* Trace is first in chain. Easy. */
    pt->trace = T->nextroot;
  } else if (pt->trace) {  /* Otherwise search in chain of root traces. */
    GCtrace *T2 = traceref(J, pt->trace);
    if (T2) {
      for (; T2->nextroot; T2 = traceref(J, T2->nextroot))
        if (T2->nextroot == T->traceno) {
          T2->nextroot = T->nextroot;  /* Unlink from chain. */
          break;
        }
    }
  }
}

/* Flush a trace. Only root traces are considered. */
void lj_trace_flush(jit_State *J, TraceNo traceno)
{
  if (traceno > 0 && traceno < J->sizetrace) {
    GCtrace *T = traceref(J, traceno);
    if (T && T->root == 0)
      trace_flushroot(J, T);
  }
}

/* Flush all traces associated with a prototype. */
void lj_trace_flushproto(global_State *g, GCproto *pt)
{
  while (pt->trace != 0)
    trace_flushroot(G2J(g), traceref(G2J(g), pt->trace));
}

/* Flush all traces. */
int lj_trace_flushall(lua_State *L) {
  jit_State *J = L2J(L);
  ptrdiff_t i;
  if ((J2G(J)->hookmask & HOOK_GC)) {
    return 1;
  }

  J->nflushall++;
  for (i = (ptrdiff_t)J->sizetrace-1; i > 0; i--) {
    GCtrace *T = traceref(J, i);
    if (T) {
      if (T->root == 0) {
        trace_flushroot(J, T);
      }
      uj_gdbjit_deltrace(J, T);
      uj_vtunejit_deltrace(J, T);
      T->traceno = 0;
      J->trace[i] = NULL;
    }
  }
  J->cur.traceno = 0;
  J->freetrace = 0;
  /* Clear penalty cache. */
  memset(J->penalty, 0, sizeof(J->penalty));
  /* Free the whole machine code and invalidate all exit stub groups. */
  lj_mcode_free(J);
  memset(J->exitstubgroup, 0, sizeof(J->exitstubgroup));
  uj_dump_compiler_progress(JSTATE_TRACE_FLUSH, J, NULL);
  return 0;
}

/* Initialize JIT compiler state. */
void lj_trace_initstate(global_State *g)
{
  jit_State *J = G2J(g);
  /* Initialize SIMD constants.
  ** hi QWORD of SIMD constants are never used in machine code for IR_ABS and
  ** IR_NEG instructions, we operate only with lower QWORD for doing math on
  ** lua_Number's (=doubles). So we use this area for storing TValue's tag.
  */
  setrawV(LJ_KSIMD(J, LJ_KSIMD_ABS), LJ_SIGN_MASK_INVERTED);
  setrawV(LJ_KSIMD(J, LJ_KSIMD_NEG), LJ_SIGN_MASK);
}

/* Free everything associated with the JIT compiler state. */
void lj_trace_freestate(global_State *g)
{
  jit_State *J = G2J(g);
#ifndef NDEBUG
  {  /* This assumes all traces have already been freed. */
    ptrdiff_t i;
    for (i = 1; i < (ptrdiff_t)J->sizetrace; i++)
      lua_assert(i == (ptrdiff_t)J->cur.traceno || traceref(J, i) == NULL);
  }
#endif
  lj_mcode_free(J);
  lj_ir_k64_freeall(J);
  uj_mem_free(MEM_G(g), J->snapmapbuf, J->sizesnapmap * sizeof(SnapEntry));
  uj_mem_free(MEM_G(g), J->snapbuf, J->sizesnap * sizeof(SnapShot));
  uj_mem_free(MEM_G(g), J->irbuf + J->irbotlim, (J->irtoplim - J->irbotlim) * sizeof(IRIns));
  uj_mem_free(MEM_G(g), J->trace, J->sizetrace * sizeof(GCtrace *));
}

/* -- Penalties and blacklisting ------------------------------------------ */

/* Penalize a bytecode instruction. */
static void penalty_pc(jit_State *J, GCproto *pt, BCIns *pc, TraceError e)
{
  uint32_t i, val = PENALTY_MIN;
  for (i = 0; i < PENALTY_SLOTS; i++)
    if (J->penalty[i].pc == pc) {  /* Cache slot found? */
      /* First try to bump its hotcount several times. */
      val = ((uint32_t)J->penalty[i].val << 1) +
            LJ_PRNG_BITS(J, PENALTY_RNDBITS);
      if (val > PENALTY_MAX) {
        uj_proto_blacklist_ins(pt, pc); /* Blacklist it, if that didn't help. */
        return;
      }
      goto setpenalty;
    }
  /* Assign a new penalty cache slot. */
  i = J->penaltyslot;
  J->penaltyslot = (J->penaltyslot + 1) & (PENALTY_SLOTS-1);
  J->penalty[i].pc = pc;
setpenalty:
  J->penalty[i].val = (uint16_t)val;
  J->penalty[i].reason = e;
  uj_hotcnt_set_counter(pc, val);
}

/* -- Trace compiler state machine ---------------------------------------- */

/* Make compiler idle and move to normal bytecode execution by VM. */
static void trace_idle(jit_State *J)
{
  J->state = LJ_TRACE_IDLE;
  uj_state_remove_event(J->L, EXTEV_JIT_ACTIVE);
  uj_dispatch_update(J2G(J));
}

/* Start tracing. */
static void trace_start(jit_State *J, TraceNo traceno)
{
  lua_assert(J->pt != NULL);
  lua_assert(traceno != 0);

  J->state = LJ_TRACE_RECORD;
  J->trace[traceno] = &J->cur;

  /* Setup enough of the current trace to be able to track progress. */
  memset(&J->cur, 0, sizeof(J->cur));
  J->cur.traceno = traceno;
  J->cur.nins = J->cur.nk = REF_BASE;
  J->cur.ir = J->irbuf;
  J->cur.snap = J->snapbuf;
  J->cur.snapmap = J->snapmapbuf;
  J->mergesnap = 0;
  J->needsnap = 0;
  J->bcskip = 0;
  J->guardemit.irt = 0;
  J->postproc = LJ_POST_NONE;
  J->cur.startpt = J->pt;

  uj_dump_compiler_progress(JSTATE_TRACE_START, J, NULL);
  lj_record_setup(J);
}

/*
 * Tries to start tracing. On success, changes J->state to LJ_TRACE_RECORD and
 * returns 1. Otherwise leaves J->state intact and returns 0.
 */
static int trace_try_start(jit_State *J)
{
  TraceNo traceno;

  if (uj_state_has_event(J->L, EXTEV_TIMEOUT_FUNC)) {
    return 0;
  }

  if (uj_proto_jit_disabled(J->pt)) {
    return 0;
  }

  /* Get a new trace number. */
  traceno = trace_findfree(J);
  if (LJ_UNLIKELY(traceno == 0)) {  /* No free trace? */
    lua_assert((J2G(J)->hookmask & HOOK_GC) == 0);
    lj_trace_flushall(J->L);
    return 0;
  }

  trace_start(J, traceno);
  return 1;
}

/* Stop tracing. */
static void trace_stop(jit_State *J)
{
  BCIns *pc = J->cur.startpc;
  BCOp op = bc_op(J->cur.startins);
  GCproto *pt = J->cur.startpt;
  TraceNo traceno = J->cur.traceno;
  GCtrace *T = trace_save_alloc(J);  /* Do this first. May throw OOM. */

  switch (op) {
  case BC_FORL:
    setbc_op(pc+bc_j(J->cur.startins), BC_JFORI);  /* Patch FORI, too. */
    /* fallthrough */
  case BC_LOOP:
  case BC_ITERL:
  case BC_ITRNL:
  case BC_FUNCF:
    /* Patch bytecode of starting instruction in root trace. */
    setbc_op(pc, (int)op+(int)BC_JLOOP-(int)BC_LOOP);
    setbc_d(pc, traceno);
  addroot:
    /* Add to root trace chain in prototype. */
    J->cur.nextroot = pt->trace;
    pt->trace = (TraceNo1)traceno;
    break;
  case BC_RET:
  case BC_RET0:
  case BC_RET1:
    *pc = BCINS_AD(BC_JLOOP, J->cur.snap[0].nslots, traceno);
    goto addroot;
  case BC_JMP: {
    /* Patch exit branch in parent to side trace entry. */
    GCtrace *T = traceref(J, J->parent);
    lua_assert(T != 0 && J->cur.root != 0);
    lj_asm_patchexit(J, T, J->exitno, J->cur.mcode);
    uj_vtunejit_updtrace(J, T);
    /* Avoid compiling a side trace twice (stack resizing uses parent exit). */
    T->snap[J->exitno].count = SNAPCOUNT_DONE;
    /* Add to side trace chain in root trace. */
    GCtrace *root = traceref(J, J->cur.root);
    root->nchild++;
    J->cur.nextside = root->nextside;
    root->nextside = (TraceNo1)traceno;
    break;
  }
  default:
    lua_assert(0);
    break;
  }

  /* Commit new mcode only after all patching is done. */
  lj_mcode_commit(J, J->cur.mcode);
  J->postproc = LJ_POST_NONE;
  trace_save(J, T);

  uj_dump_compiler_progress(JSTATE_TRACE_STOP, J, J->trace[traceno]);
}

/* Start a new root trace for down-recursion. */
static int trace_downrec(jit_State *J)
{
  /* Restart recording at the return instruction. */
  lua_assert(J->pt != NULL);
  lua_assert(bc_isret(bc_op(*J->pc)));
  if (bc_op(*J->pc) == BC_RETM)
    return 0;  /* NYI: down-recursion with RETM. */
  J->parent = 0;
  J->exitno = 0;
  return trace_try_start(J);
}

/* Abort tracing. */
static int trace_abort(jit_State *J)
{
  lua_State *L = J->abortstate.L;
  TraceError e = LJ_TRERR_RECERR;
  TraceNo traceno;

  J->postproc = LJ_POST_NONE;
  lj_mcode_abort(J);
  if (tvisnum(L->top-1))
    e = (TraceError)lj_num2int(numV(L->top-1));
  if (e == LJ_TRERR_MCODELM) {
    L->top--;  /* Remove error object */
    J->state = LJ_TRACE_ASM;
    return 1;  /* Retry ASM with new MCode area. */
  }
  /* Penalize or blacklist starting bytecode instruction. */
  if (J->parent == 0 && !bc_isret(bc_op(J->cur.startins)))
    penalty_pc(J, J->cur.startpt, J->cur.startpc, e);

  /* Is there anything to abort? */
  traceno = J->cur.traceno;
  if (traceno) {
    J->cur.link = 0;
    J->cur.linktype = LJ_TRLINK_NONE;
    uj_dump_compiler_progress(JSTATE_TRACE_ABORT, J, &e);
    /* Drop aborted trace after the dumping progress (which may still access it). */
    J->trace[traceno] = NULL;
    if (traceno < J->freetrace)
      J->freetrace = traceno;
    J->cur.traceno = 0;
  }
  L->top--;  /* Remove error object */
  if (e == LJ_TRERR_DOWNREC)
    return trace_downrec(J);
  else if (e == LJ_TRERR_MCODEAL)
    lj_trace_flushall(L);
  return 0;
}

/* Perform pending re-patch of a bytecode instruction. */
static LJ_AINLINE void trace_pendpatch(jit_State *J, int force)
{
  if (LJ_UNLIKELY(J->patchpc)) {
    if (force || J->bcskip == 0) {
      *J->patchpc = J->patchins;
      J->patchpc = NULL;
    } else {
      J->bcskip = 0;
    }
  }
}

/* State machine for the trace compiler. Protected callback. */
static TValue *trace_state(lua_State *L, lua_CFunction dummy, void *ud)
{
  jit_State *J    = (jit_State *)ud;
  global_State *g = J2G(J);
  struct vmstate_context vmsc;
  uj_vmstate_save(g->vmstate, &vmsc);
  UNUSED(dummy);
  do {
  retry:
    switch (J->state) {
    case LJ_TRACE_START:
      uj_state_add_event(J->L, EXTEV_JIT_ACTIVE);
      if (trace_try_start(J) == 0) {
        trace_idle(J);
        return NULL;
      }
      uj_dispatch_update(J2G(J));
      break;

    case LJ_TRACE_RECORD:
      trace_pendpatch(J, 0);
      uj_vmstate_set(&(J2G(J)->vmstate), UJ_VMST_RECORD);
      uj_dump_compiler_progress(JSTATE_TRACE_RECORD, J, NULL);
      lj_record_ins(J);
      break;

    case LJ_TRACE_END:
      trace_pendpatch(J, 1);
      J->loopref = 0;
      if ((J->flags & JIT_F_OPT_LOOP) &&
          J->cur.link == J->cur.traceno && J->framedepth + J->retdepth == 0) {
        uj_vmstate_set(&(J2G(J)->vmstate), UJ_VMST_OPT);
        lj_opt_dce(J);
        if (lj_opt_loop(J)) {  /* Loop optimization failed? */
          J->cur.link = 0;
          J->cur.linktype = LJ_TRLINK_NONE;
          J->loopref = J->cur.nins;
          J->state = LJ_TRACE_RECORD;  /* Try to continue recording. */
          break;
        }
        J->loopref = J->chain[IR_LOOP];  /* Needed by assembler. */
      }
      lj_opt_sink(J);
      lj_opt_movtv(J);
      if (!J->loopref) { J->cur.snap[J->cur.nsnap-1].count = SNAPCOUNT_DONE; }
      J->state = LJ_TRACE_ASM;
      break;

    case LJ_TRACE_ASM:
      uj_vmstate_set(&(J2G(J)->vmstate), UJ_VMST_ASM);
      lj_asm_trace(J, &J->cur);
      trace_stop(J);
      uj_vmstate_restore(&g->vmstate, &vmsc);
      trace_idle(J);
      return NULL;

    default:  /* Trace aborted asynchronously. */
      setintV(L->top++, (int32_t)LJ_TRERR_RECERR);
      /* fallthrough */
    case LJ_TRACE_ERR:
      trace_pendpatch(J, 1);
      if (trace_abort(J))
        goto retry;
      uj_vmstate_restore(&g->vmstate, &vmsc);
      trace_idle(J);
      return NULL;
    }
  } while (J->state > LJ_TRACE_RECORD);
  return NULL;
}

/* -- Event handling ------------------------------------------------------ */

/* A bytecode instruction is about to be executed. Record it. */
void lj_trace_ins(jit_State *J, const BCIns *pc)
{
  /* Note: J->L must already be set. pc is the true bytecode PC here. */
#ifndef NDEBUG
  ptrdiff_t delta = J->L->top - J->L->base;
#endif /* !NDEBUG */
  J->pc = pc;
  J->fn = curr_func(J->L);
  J->pt = isluafunc(J->fn) ? funcproto(J->fn) : NULL;
  while (lj_vm_cpcall(J->L, NULL, (void *)J, trace_state) != 0)
    J->state = LJ_TRACE_ERR;
  lua_assert(J->L->top - J->L->base == delta);
}

/* A hotcount triggered. Start recording a root trace. */
void lj_trace_hot(jit_State *J, const BCIns *pc)
{
  /* Note: pc is the interpreter bytecode PC here. It's offset by 1. */
  int olderr = errno_save();
#ifndef NDEBUG
  ptrdiff_t delta = J->L->top - J->L->base;
#endif /* !NDEBUG */
  /* Reset hotcount. */
  uj_hotcnt_set_counter((BCIns *)pc - 1, J->param[JIT_P_hotloop]);
  /* Only start a new trace if not recording or inside __gc call. */
  if (J->state == LJ_TRACE_IDLE && !(J2G(J)->hookmask & HOOK_GC)) {
    J->parent = 0;  /* Root trace. */
    J->exitno = 0;
    J->state = LJ_TRACE_START;
    lj_trace_ins(J, pc-1);
  }
  lua_assert(J->L->top - J->L->base == delta);
  errno_restore(olderr);
}

/* Check for a hot side exit. If yes, start recording a side trace. */
static void trace_hotside(jit_State *J, const BCIns *pc) {
  GCtrace  *T    = traceref(J, J->parent);
  SnapShot *snap = &T->snap[J->exitno];
  if (J2G(J)->hookmask & HOOK_GC) {
    return;
  }
  /* No need to record if side exit points to an already compiled trace */
  if (snap->count == SNAPCOUNT_DONE) {
    return;
  }
  if (++snap->count >= J->param[JIT_P_hotexit]) {
    lua_assert(J->state == LJ_TRACE_IDLE);
    /* J->parent is non-zero for a side trace. */
    J->state = LJ_TRACE_START;
    lj_trace_ins(J, pc);
  }
}

/* Tiny struct to pass data to protected call. */
typedef struct ExitDataCP {
  jit_State *J;
  void *exptr;  /* Pointer to exit state. */
  BCIns *pc;    /* Restart interpreter at this PC. */
} ExitDataCP;

/* Need to protect lj_snap_restore because it may throw. */
static TValue *trace_exit_cp(lua_State *L, lua_CFunction dummy, void *ud)
{
  ExitDataCP *exd = (ExitDataCP *)ud;
  uj_cframe_errfunc_inherit(L->cframe);  /* Inherit error function. */
  exd->pc = lj_snap_restore(exd->J, exd->exptr);
  UNUSED(dummy);
  return NULL;
}

/* A trace exited. Restore interpreter state. */
int lj_trace_exit(jit_State *J, void *exptr)
{
  int olderr = errno_save();
  lua_State *L = J->L;
  ExitState *ex = (ExitState *)exptr;
  ExitDataCP exd;
  int errcode;
  BCIns *pc;
  void *cf;
  GCtrace *T;
  T = traceref(J, J->parent); UNUSED(T);
  lua_assert(T != NULL && J->exitno < T->nsnap);
  exd.J = J;
  exd.exptr = exptr;
  errcode = lj_vm_cpcall(L, NULL, &exd, trace_exit_cp);
  if (errcode)
    return -errcode;  /* Return negated error code. */

  uj_dump_compiler_progress(JSTATE_TRACE_EXIT, J, ex);

  pc = exd.pc;
  cf = uj_cframe_raw(L->cframe);
  uj_cframe_pc_set(cf, pc);
  if (G(L)->gc.state == GCSatomic || G(L)->gc.state == GCSfinalize) {
    if (!(G(L)->hookmask & HOOK_GC))
      lj_gc_step(L);  /* Exited because of GC: drive GC forward. */
  } else {
    trace_hotside(J, pc);
  }
  if (bc_op(*pc) == BC_JLOOP) {
    BCIns *retpc = &traceref(J, bc_d(*pc))->startins;
    if (bc_isret(bc_op(*retpc))) {
      if (J->state == LJ_TRACE_RECORD) {
        J->patchins = *pc;
        J->patchpc = (BCIns *)pc;
        *J->patchpc = *retpc;
        J->bcskip = 1;
      } else {
        pc = retpc;
        uj_cframe_pc_set(cf, pc);
      }
    }
  }
  /* Return MULTRES or 0. */
  errno_restore(olderr);
  switch (bc_op(*pc)) {
  case BC_CALLM: case BC_CALLMT:
    return (int)((BCReg)(L->top - L->base) - bc_a(*pc) - bc_c(*pc));
  case BC_RETM:
    return (int)((BCReg)(L->top - L->base) + 1 - bc_a(*pc) - bc_d(*pc));
  case BC_TSETM:
    return (int)((BCReg)(L->top - L->base) + 1 - bc_a(*pc));
  default:
    if (bc_op(*pc) >= BC_FUNCF)
      return (int)((BCReg)(L->top - L->base) + 1);
    return 0;
  }
}

#endif
