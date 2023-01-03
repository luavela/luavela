/*
 * MOVTV: Copying TValue's without guarded loads.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "jit/lj_jit.h"
#include "lj_bc.h"

static LJ_AINLINE IRIns *IR(const jit_State *J, IRRef ref)
{
	return &J->cur.ir[(ref)];
}

static LJ_AINLINE int movtv_is_load(const IRIns *ir)
{
	return ir->o == IR_ALOAD || ir->o == IR_HLOAD;
}

static LJ_AINLINE int movtv_is_store(const IRIns *ir)
{
	return ir->o == IR_ASTORE || ir->o == IR_HSTORE;
}

static LJ_AINLINE int movtv_is_tget(const BCIns *ins)
{
	BCOp op = bc_op(*ins);

	return op == BC_TGETV || op == BC_TGETS || op == BC_TGETB ||
	       op == BC_GGET;
}

static LJ_AINLINE int movtv_is_tset(const BCIns *ins)
{
	BCOp op = bc_op(*ins);

	return op == BC_TSETV || op == BC_TSETS || op == BC_TSETB ||
	       op == BC_TSETM || op == BC_GSET;
}

#define MAX_CHAIN_LENGTH 16

static LJ_AINLINE IRRef movtv_tab(const jit_State *J, const IRIns *ir)
{
	IRRef ref;
	size_t chain;

	lua_assert(movtv_is_load(ir) || movtv_is_store(ir));

	/*
	 * NB! We rely on the fact that a chain of op1's for any load or store
	 * will lead to some IRT_TAB instruction. A safety net: If the number of
	 * lookups is too large, we give up and return an invalid reference.
	 * This is a clear signal about a broken IR chain.
	 */
	ref = ir->op1;
	chain = 0;
	while (!irt_istab(IR(J, ref)->t)) {
		ref = IR(J, ref)->op1;
		if (++chain > MAX_CHAIN_LENGTH)
			return 0;
	}

	/*
	 * And here is the list of sources of IRT_TAB on the platform.
	 * The current idea is that it is better to crash during testing than
	 * degrade silently as more and more IRs appear.
	 */
	lua_assert(IR(J, ref)->o == IR_KGC || IR(J, ref)->o == IR_HKLOAD ||
		   IR(J, ref)->o == IR_ALOAD || IR(J, ref)->o == IR_HLOAD ||
		   IR(J, ref)->o == IR_ULOAD || IR(J, ref)->o == IR_FLOAD ||
		   IR(J, ref)->o == IR_SLOAD || IR(J, ref)->o == IR_VLOAD ||
		   IR(J, ref)->o == IR_TVLOAD || IR(J, ref)->o == IR_TNEW ||
		   IR(J, ref)->o == IR_TDUP || IR(J, ref)->o == IR_CALLN ||
		   IR(J, ref)->o == IR_CALLS);

	return ref;
}

static int movtv_is_plain(const jit_State *J, const IRIns *ir)
{
	IRRef ref = movtv_tab(J, ir);

	if (!ref) /* unable to deduce ir's table */
		return 0;

	/*
	 * Unfortunately, we cannot tell much about KGCs as we do not store
	 * hints about them, but this does not look like a hot case anyway.
	 */
	if (irref_isk(ref))
		return 0;

	if (ir_hashint(IR(J, ref), IRH_TAB_METAIDX))
		return 0;

	if (ir_hashint(IR(J, ref), IRH_TAB_SETMETA))
		return 0;

	return 1;
}

static void movtv_assert_stack(jit_State *J)
{
#ifndef NDEBUG
	BCReg s;
	BCReg nslots = J->baseslot + J->maxslot;

	for (s = 0; s < nslots; s++) {
		TRef tr = J->slot[s];

		lua_assert(!tr || !irt_ismarked(IR(J, tref_ref(tr))->t));
	}
#else /* NDEBUG */
	UNUSED(J);
#endif /* !NDEBUG */
}

/*
 * Unmark all instructions, but put an extra mark on all memory
 * references that are referenced by at least one store.
 */
static void movtv_init(jit_State *J)
{
	IRIns *ir;
	IRIns *ir_first = IR(J, REF_FIRST);
	IRIns *ir_last = IR(J, J->cur.nins - 1);

	for (ir = ir_last; ir >= ir_first; ir--) {
		irt_clearmark(ir->t);

		if (movtv_is_store(ir) && !irref_isk(ir->op1))
			ir_sethint(IR(J, ir->op1), IRH_MARK);
	}
}

/*
 * Mark all optimizable loads. Each optimizable load...
 * * ...must be reachable from a non-sunk store to a table without a metatable;
 * * ...must not have the IRH_LOAD_KEEPGUARD hint;
 * * ...must originate from a table without a metatable.
 * semantics and referenced only by stores.
 */
static void movtv_mark_loads(jit_State *J)
{
	IRIns *ir;
	IRIns *ir_first = IR(J, REF_FIRST);
	IRIns *ir_last = IR(J, J->cur.nins - 1);

	for (ir = ir_last; ir >= ir_first; ir--) {
		IRIns *load;

		if (!movtv_is_store(ir))
			continue;

		if (!movtv_is_plain(J, ir))
			continue;

		if (ir->r == RID_SINK)
			continue;

		load = IR(J, ir->op2);

		if (!movtv_is_load(load))
			continue;

		if (ir_hashint(load, IRH_LOAD_KEEPGUARD))
			continue;

		if (!movtv_is_plain(J, load))
			continue;

		irt_setmark(load->t);
	}
}

/*
 * Filter: Unmark all loads that escape to snapshots.
 *
 * NB! During snapshot scanning we de facto perform a linear scan of
 * the entire snapmap. Cannot find a more efficient way to do this check :-(
 */
static void movtv_filter_loads(jit_State *J)
{
	size_t i, j;

	for (i = 0; i < J->cur.nsnap; i++) {
		const SnapShot *snap = &J->cur.snap[i];
		const SnapEntry *map = &J->cur.snapmap[snap->mapofs];

		for (j = 0; j < snap->nent; j++) {
			IRRef ref = snap_ref(map[j]);

			if (ref)
				irt_clearmark(IR(J, ref)->t);
		}
	}

	movtv_assert_stack(J);
}

/*
 * Multi-pass finalization:
 *
 * Finalization of stores that reference marked loads:
 *  * Set MOVTV hint for the store and "its" table.
 *  * Adjust IR return type.
 *  * If possible, fuse load into store. The criterion: if there are
 *    no on-trace stores to the load's memory reference (=no extra
 *    mark set on init step), the load can be fused.
 *
 * Finalization of marked loads:
 *  * Set MOVTV hint for the store and "its" table.
 *  * If load cannot be fused, adjust IR return type.
 *  * Drop assertion guard and all marks.
 *
 * Clean-up: Drop the leftover marks
 */
static void movtv_finalize(jit_State *J)
{
	IRIns *ir;
	IRIns *ir_first = IR(J, REF_FIRST);
	IRIns *ir_last = IR(J, J->cur.nins - 1);

	for (ir = ir_last; ir >= ir_first; ir--) {
		IRIns *load;
		IRRef tab;

		if (!movtv_is_store(ir))
			continue;

		load = IR(J, ir->op2);

		if (!irt_ismarked(load->t))
			continue;

		lua_assert(movtv_is_load(load));
		lua_assert(IR(J, load->op1)->o == IR_AREF ||
			   IR(J, load->op1)->o == IR_HREF ||
			   IR(J, load->op1)->o == IR_HREFK ||
			   IR(J, load->op1)->o == IR_NEWREF);

		if (!ir_hashint(IR(J, load->op1), IRH_MARK))
			ir->op2 = load->op1;
		else
			ir_sethint(load, IRH_MARK); /* aux for the next pass */

		tab = movtv_tab(J, ir);
		lua_assert(tab);

		ir->t.irt = (ir->t.irt & ~IRT_TYPE) | IRT_TVAL;
		ir_sethint(ir, IRH_MOVTV);
		ir_sethint(IR(J, tab), IRH_MOVTV);
	}

	for (ir = ir_last; ir >= ir_first; ir--) {
		IRRef tab;

		if (!irt_ismarked(ir->t))
			continue;

		lua_assert(movtv_is_load(ir));

		irt_clearmark(ir->t);
		ir->t.irt &= ~IRT_GUARD;

		if (ir_hashint(ir, IRH_MARK)) {
			ir_clearhint(ir, IRH_MARK);
			ir->t.irt = (ir->t.irt & ~IRT_TYPE) | IRT_TVAL;
		} /* else: load will be eliminated by DCE */

		tab = movtv_tab(J, ir);
		lua_assert(tab);

		ir_sethint(ir, IRH_MOVTV);
		ir_sethint(IR(J, tab), IRH_MOVTV);
	}

	for (ir = ir_last; ir >= ir_first; ir--) {
		if (movtv_is_store(ir) && !irref_isk(ir->op1))
			ir_clearhint(IR(J, ir->op1), IRH_MARK);

		lua_assert(!ir_hashint(ir, IRH_MARK));
	}
}

static LJ_AINLINE int movtv_applicable(const jit_State *J)
{
	const uint32_t need = JIT_F_OPT_DCE | JIT_F_OPT_MOVTV;

	return (J->flags & need) == need;
}

/*
 * Give the compiler a hint whether an IR referenced by tr can be a subject
 * to the MOVTV optimization. If IR is a load and if recording of any bytecode
 * rather than GSET/TSET* depends on the IR, MOVTV is definitely not applicable
 * and the load must be kept guarded. Otherwise the IR may be considered by
 * the optimization.
 *
 * Inspecting relations between bytecodes and IRs during recording gives us
 * maximum information. Checking at the IR level is not enough because there are
 * cases when a bytecode is recorded with 0 IRs (e.g. BC_IST): "recording"
 * simply relies on guards emitted earlier.
 *
 * This function returns TRef for convenience.
 */
TRef lj_opt_movtv_rec_hint(jit_State *J, TRef tr)
{
	IRIns *ir;

	if (!movtv_applicable(J))
		return tr;

	if (tref_isk(tr))
		return tr;

	ir = IR(J, tref_ref(tr));

	if (!movtv_is_load(ir))
		return tr;

	if (!movtv_is_tset(J->pc)) {
		ir_sethint(ir, IRH_LOAD_KEEPGUARD);
		if (tref_ispri(tr)) {
			int is_canon = tr == TREF_NIL || tr == TREF_FALSE ||
				       tr == TREF_TRUE;

			if (!is_canon)
				tr = TREF_PRI(tr);
		}
	}

	return tr;
}

/*
 * By default recorder does its best to intern primitive values (nil, false,
 * true) and canonicalize corresponding IR references, which results in such
 * chains as:
 *
 * > tru LOAD ref
 * <...>
 *   tru STORE ref tru
 *
 * > EQ ref [nilnode]
 *   nil STORE ref nil
 *
 * Such chains are obviously cannot be detected by MOVTV. We use a simple
 * heuristics to detect cases when canonicalization can possibly be deferred:
 * There should be a BC_TGET* adjacent to BC_TSET* with a common RA operand.
 * Later, if this RA is referenced by a non-BC_TSET*, references are
 * canonicalized during lj_opt_movtv_rec_hint (and guarded loads preserve
 * their guards anyway).
 */
int lj_opt_movtv_defer_canon(const jit_State *J)
{
	if (!movtv_applicable(J))
		return 0;

	if (!(J->flags & JIT_F_OPT_MOVTVPRI))
		return 0;

	if (!movtv_is_tget(J->pc))
		return 0;

	if (!movtv_is_tset(J->pc + 1))
		return 0;

	if (bc_a(*(J->pc)) != bc_a(*(J->pc + 1)))
		return 0;

	return 1;
}

void lj_opt_movtv(jit_State *J)
{
	if (!movtv_applicable(J))
		return;

	movtv_init(J);
	movtv_mark_loads(J);
	movtv_filter_loads(J);
	movtv_finalize(J);
}
