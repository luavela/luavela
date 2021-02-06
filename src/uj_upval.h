/*
 * Upvalue handling.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJ_UPVAL_H
#define _UJ_UPVAL_H

#include "lj_obj.h"

static LJ_AINLINE uint32_t uj_upval_dhash(const void *obj, uint32_t idx)
{
	return (uint32_t)(uintptr_t)obj ^ (idx << 24);
}

/* Create an empty and closed upvalue. */
GCupval *uj_upval_new_closed_empty(lua_State *L);

/* Find existing open upvalue for a stack slot or create a new one. */
GCupval *uj_upval_find(lua_State *L, TValue *slot);

void uj_upval_free(global_State *g, GCupval *uv);

/* Close all open upvalues pointing to some stack level or above. */
void uj_upval_close(lua_State *L, TValue *level);

#endif /* !_UJ_UPVAL_H */
