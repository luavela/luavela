/*
 * Implementation of immutability.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "lj_obj.h"
#include "uj_obj_immutable.h"
#include "lj_tab.h"
#include "uj_err.h"

static void immutable_mark(lua_State *L, GCobj *o);

static void immutable_traverse(lua_State *L, GCobj *o, gco_mark_flipper marker)
{
	switch (o->gch.gct) {
	case (~LJ_TTAB): {
		lj_tab_traverse(L, gco2tab(o), marker);
		return;
	}
	case (~LJ_TPROTO):
	case (~LJ_TFUNC):
	case (~LJ_TSTR):
	case (~LJ_TUDATA): {
		lua_assert(0); /* Should have been set immutable on creation. */
		return;
	}
	default: { /* NYI */
		lua_assert(
			o->gch.gct == ~LJ_TUPVAL || o->gch.gct == ~LJ_TTHREAD ||
			o->gch.gct == ~LJ_TTRACE || o->gch.gct == ~LJ_TCDATA);
		break;
	}
	}
	if (marker == immutable_mark)
		uj_err_gco(L, UJ_ERR_IMMUT_BADTYPE, o);
}

static void immutable_mark(lua_State *L, GCobj *o)
{
	if (uj_obj_is_immutable(o))
		return;

	uj_obj_propagate_set(L, o, immutable_traverse, immutable_mark);
}

static void immutable_unmark(lua_State *L, GCobj *o)
{
	if (uj_obj_is_immutable(o))
		return;

	uj_obj_propagate_clear(L, o, immutable_traverse, immutable_unmark);
}

static void immutable_commit(lua_State *L, GCobj *o)
{
	if (uj_obj_is_immutable(o))
		return;

	lua_assert(lj_obj_has_mark(o));
	lj_obj_clear_mark(o);
	uj_obj_immutable_set_mark(o);
	immutable_traverse(L, o, immutable_commit);
}

void uj_obj_immutable(lua_State *L, GCobj *o)
{
	if (uj_obj_is_immutable(o))
		return;

	uj_obj_pmark(L, o, immutable_mark, immutable_unmark);
	immutable_commit(L, o);
}
