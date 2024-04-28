/*
 * Userdata handling.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "lj_obj.h"
#include "uj_obj_immutable.h"
#include "uj_mem.h"
#include "lj_gc.h"

GCudata *uj_udata_new(lua_State *L, size_t sz, GCtab *env)
{
	GCudata *ud = (GCudata *)uj_mem_alloc(L, sizeof(GCudata) + sz);
	global_State *g = G(L);

	lua_assert(checku32(sz));

	newwhite(g, ud); /* Not finalized. */
	uj_obj_immutable_set_mark(obj2gco(ud));
	ud->gct = ~LJ_TUDATA;
	ud->udtype = UDTYPE_USERDATA;
	ud->len = (uint32_t)sz;
	/* NOBARRIER: The GCudata is new (marked white). */
	ud->metatable = NULL;
	ud->env = env;
	ud->nextgc = mainthread(g)->nextgc;
	mainthread(g)->nextgc = obj2gco(ud);
	g->gc.udatanum++;
	return ud;
}

size_t uj_udata_sizeof(const GCudata *ud)
{
	return sizeof(GCudata) + ud->len;
}

void uj_udata_free(global_State *g, GCudata *ud)
{
	g->gc.udatanum--;
	uj_mem_free(MEM_G(g), ud, uj_udata_sizeof(ud));
}
