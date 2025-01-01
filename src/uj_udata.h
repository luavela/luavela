/*
 * Userdata handling.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _UJ_UDATA_H
#define _UJ_UDATA_H

#include "lj_def.h"

struct global_State;
struct lua_State;
struct GCtab;
struct GCudata;

GCudata *uj_udata_new(lua_State *L, size_t sz, GCtab *env);
size_t uj_udata_sizeof(const GCudata *ud);
void uj_udata_free(global_State *g, GCudata *ud);

#endif /* !_UJ_UDATA_H */
