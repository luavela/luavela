/*
 * Hook management interfaces.
 * NB! An important note about wording in this module:
 * Please use the verb "invoke" to denote "start an execution of an arbitrary
 * hook". This allows to avoid confusion with the term "call hook", a certain
 * hook type which has to be *invoked* whenever e.g. some Lua code
 * *calls* another Lua code.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "uj_hook.h"
#include "uj_state.h"
#include "lj_bc.h"
#include "uj_cframe.h"
#include "uj_dispatch.h"
#include "uj_proto.h"
#if LJ_HASJIT
#include "jit/lj_trace.h"
#endif /* LJ_HASJIT */

/* -- Assertions ---------------------------------------------------------- */

#if LJ_HASJIT

#ifndef NDEBUG
static ptrdiff_t hook_assert_gettop(const struct lua_State *L)
{
	return L->top - L->base;
}

static void hook_assert_framesz(const struct lua_State *L, ptrdiff_t sz)
{
	lua_assert(hook_assert_gettop(L) == sz);
}

#else /* NDEBUG */

static ptrdiff_t hook_assert_gettop(const struct lua_State *L)
{
	UNUSED(L);
	return 0;
}

static void hook_assert_framesz(const struct lua_State *L, ptrdiff_t sz)
{
	UNUSED(L);
	UNUSED(sz);
}

#endif /* !NDEBUG */

#endif /* LJ_HASJIT */

/* -- Hooks --------------------------------------------------------------- */

/* Invoke an arbitrary hook. */
static void hook_invoke(struct lua_State *L, int event, BCLine line)
{
	lua_Debug ar;
	struct global_State *g = G(L);
	lua_Hook hookf = g->hookf;

	if (!hookf || hook_active(g))
		return;

#if LJ_HASJIT
	lj_trace_abort(g); /* Abort recording on any hook call. */
#endif
	ar.event = event;
	ar.currentline = line;
	/* Top frame, nextframe = NULL. */
	ar.i_ci = (int)((L->base - 1) - L->stack);
	uj_state_stack_check(L, 1 + LUA_MINSTACK);
	hook_enter(g);
	hookf(L, &ar);
	lua_assert(hook_active(g));
	hook_leave(g);
}

/* -- Dispatch callbacks -------------------------------------------------- */

/* Calculate number of used stack slots in the current frame. */
static BCReg hook_cur_topslot(const struct GCproto *pt, const BCIns *pc,
			      uint32_t nres)
{
	BCIns ins = pc[-1];

	if (bc_op(ins) == BC_UCLO)
		ins = pc[bc_j(ins)];
	switch (bc_op(ins)) {
	case BC_CALLM:
	case BC_CALLMT:
		return bc_a(ins) + bc_c(ins) + nres - 1 + 1;
	case BC_RETM:
		return bc_a(ins) + bc_d(ins) + nres - 1;
	case BC_TSETM:
		return bc_a(ins) + nres - 1;
	default:
		return pt->framesize;
	}
}

#if LJ_HASJIT
static LJ_AINLINE void hook_record_ins(struct lua_State *L, const BCIns *pc)
{
	jit_State *J = L2J(L);
	ptrdiff_t delta;

	if (J->state == LJ_TRACE_IDLE)
		return;

	delta = hook_assert_gettop(L);
	J->L = L;
	/* The interpreter bytecode PC is offset by 1. */
	lj_trace_ins(J, pc - 1);
	hook_assert_framesz(L, delta);
}

/* Trigger recording of a root trace starting with the FUNC* bytecode. */
static LJ_AINLINE void hook_record_call(struct lua_State *L, const BCIns *pc)
{
	struct jit_State *J = L2J(L);
	ptrdiff_t delta = hook_assert_gettop(L);

	J->L = L;
	lj_trace_hot(J, pc);
	hook_assert_framesz(L, delta);
}

static LJ_AINLINE BCOp hook_get_op(const struct lua_State *L, const BCIns *pc)
{
	const struct jit_State *J = L2J(L);
	BCOp op = bc_op(pc[-1]);

	/* Use the non-hotcounting variants if JIT is off or while recording. */
	if ((!(J->flags & JIT_F_ON) || J->state != LJ_TRACE_IDLE) &&
	    (op == BC_FUNCF || op == BC_FUNCV))
		op = (BCOp)((int)op + (int)BC_IFUNCF - (int)BC_FUNCF);
	return op;
}
#else /* LJ_HASJIT */
static LJ_AINLINE void hook_record_call(struct lua_State *L, const BCIns *pc)
{
	UNUSED(L);
	UNUSED(pc);
}

static LJ_AINLINE void hook_record_ins(struct lua_State *L, const BCIns *pc)
{
	UNUSED(L);
	UNUSED(pc);
}
static LJ_AINLINE BCOp hook_get_op(const struct lua_State *L, const BCIns *pc)
{
	UNUSED(L);
	return bc_op(pc[-1]);
}
#endif /* LJ_HASJIT */

/* Instruction dispatch. Used by instr/line/return hooks or when recording. */
void uj_hook_ins(struct lua_State *L, const BCIns *pc)
{
	int olderr = errno_save();
	const union GCfunc *fn = curr_func(L);
	const struct GCproto *pt = funcproto(fn);
	void *cf = uj_cframe_raw(L->cframe);
	const BCIns *oldpc = uj_cframe_pc(cf);
	struct global_State *g = G(L);
	BCReg slots;

	uj_cframe_pc_set(cf, pc);
	slots = hook_cur_topslot(pt, pc, uj_cframe_multres(cf));
	L->top = L->base + slots; /* Fix top. */
	hook_record_ins(L, pc);
	if ((g->hookmask & LUA_MASKCOUNT) && g->hookcount == 0) {
		g->hookcount = g->hookcstart;
		hook_invoke(L, LUA_HOOKCOUNT, -1);
		L->top = L->base + slots; /* Fix top again. */
	}
	if ((g->hookmask & LUA_MASKLINE)) {
		BCPos npc = proto_bcpos(pt, pc) - 1;
		BCPos opc = proto_bcpos(pt, oldpc) - 1;
		BCLine line = uj_proto_line(pt, npc);

		if (pc <= oldpc || opc >= pt->sizebc ||
		    line != uj_proto_line(pt, opc)) {
			hook_invoke(L, LUA_HOOKLINE, line);
			L->top = L->base + slots; /* Fix top again. */
		}
	}
	if ((g->hookmask & LUA_MASKRET) && bc_isret(bc_op(pc[-1])))
		hook_invoke(L, LUA_HOOKRET, -1);
	errno_restore(olderr);
}

/* Initialize call. Ensure stack space and return # of missing parameters. */
static int hook_call_init(struct lua_State *L)
{
	const union GCfunc *fn = curr_func(L);

	if (isluafunc(fn)) {
		const struct GCproto *pt = funcproto(fn);
		int numparams = pt->numparams;
		int gotparams = (int)(L->top - L->base);
		int need = pt->framesize;

		if (pt->flags & PROTO_VARARG)
			need += 1 + gotparams;
		uj_state_stack_check(L, (size_t)need);
		numparams -= gotparams;
		return numparams >= 0 ? numparams : 0;
	} else {
		uj_state_stack_check(L, LUA_MINSTACK);
		return 0;
	}
}

static void hook_call_invoke(struct lua_State *L, int missing)
{
	int i;

	for (i = 0; i < missing; i++) /* Add missing parameters. */
		setnilV(L->top++);

	hook_invoke(L, LUA_HOOKCALL, -1);

	/*
	 * Preserve modifications of missing parameters
	 * by lua_setlocal().
	 */
	while (missing-- > 0 && tvisnil(L->top - 1))
		L->top--;
}

/* Call dispatch. Used by call hooks, hot calls or when recording. */
ASMFunction uj_hook_call(struct lua_State *L, const BCIns *pc)
{
	int olderr = errno_save();
	int missing;
	BCOp op;
	const uint8_t hookmask = G(L)->hookmask;

	missing = hook_call_init(L);

	if ((uintptr_t)pc & 1) { /* Marker for hot call. */
		pc = (const BCIns *)((uintptr_t)pc & ~(uintptr_t)1);
		hook_record_call(L, pc);
		goto out;
	} else if (!(hookmask & HOOK_GC)) {
		/* Record the FUNC* bytecodes, too. */
		hook_record_ins(L, pc);
	}

	if (hookmask & LUA_MASKCALL)
		hook_call_invoke(L, missing);

out:
	op = hook_get_op(L, pc); /* Get FUNC* op */
	errno_restore(olderr);
	return lj_bc_ptr[op]; /* Return static dispatch target. */
}
