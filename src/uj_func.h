/*
 * Function handling
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _UJ_FUNC_H
#define _UJ_FUNC_H

#include "lj_obj.h"

GCfunc *uj_func_newC(lua_State *L, size_t nelems, GCtab *env);
GCfunc *uj_func_newL_empty(lua_State *L, GCproto *pt, GCtab *env);
GCfunc *uj_func_newL_gc(lua_State *L, GCproto *pt, GCfuncL *parent);
size_t uj_func_sizeof(const GCfunc *fn);
void uj_func_free(global_State *g, GCfunc *fn);

/*
 * Checks if fn uses its environment. Following logic applies:
 *  * For Lua functions, returns true if the function meets
 *    at least one of following conditions:
 *    * Its prototype accesses at least one global variable
 *    * Function itself references at least one upvalue
 *    If neither condition is met, returns false.
 *  * For built-in functions, always returns false.
 *  * For registered C functions, always returns true.
 */
int uj_func_usesfenv(const GCfunc *fn);

/* For a sealed function, propagates the seal mark to dependent objects. */
void uj_func_seal_traverse(lua_State *L, GCfunc *fn, gco_mark_flipper marker);

static LJ_AINLINE int uj_func_has_upvalues(const GCfunc *fn)
{
	return isluafunc(fn) && fn->l.nupvalues != 0;
}

#endif /* !_UJ_FUNC_H */
