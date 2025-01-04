/*
 * uJIT hotcounting mechanism based on HOTCNT byte code.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "lj_obj.h"
#include "uj_dispatch.h"
#include "uj_hotcnt.h"

#if LJ_HASJIT

/* Relative offset to the HOTCNT byte code */
static int hotcnt_offset(BCIns ins)
{
	switch (bc_op(ins)) {
	case BC_ITERL:
	case BC_ITRNL:
		return ITERL_OFFSET;
	case BC_FUNCF:
	case BC_JFUNCF:
		return -PROLOGUE_OFFSET;
	case BC_LOOP:
	case BC_FORL:
		return LOOP_FORL_OFFSET;
	default:
		lua_assert(0);
		return 0;
	}
}

static LJ_AINLINE int hotcnt_is_prologue(const BCIns *ins)
{
	return bc_op(ins[-PROLOGUE_OFFSET]) == BC_FUNCF;
}

static LJ_AINLINE int hotcnt_is_iterl(const BCIns *ins)
{
	BCOp op = bc_op(ins[ITERL_OFFSET]);
	return op == BC_ITERL || op == BC_ITRNL;
}

/* Adjust counter value for FUNCF and ITERL */
static LJ_AINLINE uint16_t hotcnt_adjust(const BCIns *ins, uint16_t val)
{
	if (hotcnt_is_prologue(ins))
		/*
		 * In luajit, hotcounters for functions are multiplied by 2.
		 * -1 is a little hack, because HOTCNT executes after FUNCF.
		 * For details, see implementation of FUNCF and ITERL, compare
		 * order there with LOOP (e.g.)
		 */
		return 2 * val - 1;
	else if (hotcnt_is_iterl(ins))
		/* -1 is the same as for FUNCF. HOTCNT executes after ITERL. */
		return val - 1;
	else
		return val;
}

void uj_hotcnt_set_counter(BCIns *bc, uint16_t val)
{
	uint16_t *counter;

	/* Now bc points to the HOTCNT */
	if (bc_op(*bc) != BC_HOTCNT)
		bc -= hotcnt_offset(*bc);

	lua_assert(bc_op(*bc) == BC_HOTCNT);

	counter = (uint16_t *)((char *)bc + COUNTER_OFFSET);
	*counter = hotcnt_adjust(bc, val);
}

void uj_hotcnt_patch_bc(GCproto *pt, uint16_t hotloop)
{
	BCIns *bc = (BCIns *)proto_bc(pt);
	BCIns *end = bc + pt->sizebc;

	while (bc != end) {
		if (bc_op(*bc) == BC_HOTCNT)
			uj_hotcnt_set_counter(bc, hotloop);
		bc++;
	}
}

void uj_hotcnt_patch_protos(global_State *g)
{
	GCobj *o;
	GCobj **iter = &g->gc.root;
	uint16_t hotloop = (uint16_t)G2J(g)->param[JIT_P_hotloop];

	while (NULL != (o = *iter)) {
		switch (o->gch.gct) {
		case (~LJ_TPROTO):
			uj_hotcnt_patch_bc((GCproto *)o, hotloop);
			break;
		default:
			break;
		}
		iter = &o->gch.nextgc;
	}
}

#else /* !LJ_HASJIT */

void uj_hotcnt_set(BCIns *bc, uint16_t val)
{
	UNUSED(bc);
	UNUSED(val);
	return;
}

void uj_hotcnt_patch_bc(GCproto *pt, uint16_t hotloop)
{
	UNUSED(pt);
	UNUSED(hotloop);
	return;
}

void uj_hotcnt_patch_proto(global_State *g)
{
	UNUSED(g);
	return;
}

#endif /* LJ_HASJIT */
