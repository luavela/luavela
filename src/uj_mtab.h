/*
 * Metatable handling.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJ_MTAB_H
#define _UJ_MTAB_H

#include "lj_obj.h"

/* Returns the value's metatable or NULL if no metatable was found. */
GCtab *uj_mtab_get(const lua_State *L, const TValue *tv);

/*
 * Returns a per-type metatable associated with the type of tv.
 * If there is no such table, returns NULL.
 */
static LJ_AINLINE GCtab *uj_mtab_get_for_otype(const global_State *g,
					       const TValue *tv)
{
	GCobj *mt = g->gcroot[GCROOT_BASEMT + ~(gettag(tv))];
	return mt != NULL ? gco2tab(mt) : NULL;
}

/* Sets mt as a per-type metatable associated with the type denoted by tag. */
static LJ_AINLINE void uj_mtab_set_for_type(global_State *g, uint32_t tag,
					    GCtab *mt)
{
	g->gcroot[GCROOT_BASEMT + ~tag] = obj2gco(mt);
}

/* Sets mt as a per-type metatable associated with the type of tv. */
static LJ_AINLINE void uj_mtab_set_for_otype(global_State *g, const TValue *tv,
					     GCtab *mt)
{
	uj_mtab_set_for_type(g, gettag(tv), mt);
}

#endif /* !_UJ_MTAB_H */
