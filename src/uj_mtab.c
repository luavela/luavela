/*
 * Implementation of metatable handling.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "lj_obj.h"
#include "uj_mtab.h"

GCtab *uj_mtab_get(const lua_State *L, const TValue *tv)
{
	if (tvistab(tv))
		return tabV(tv)->metatable;

	if (tvisudata(tv))
		return udataV(tv)->metatable;

	return uj_mtab_get_for_otype(G(L), tv);
}
