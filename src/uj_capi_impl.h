/*
 * Internal interfaces for the Lua/C API.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJ_CAPI_IMPL_H
#define _UJ_CAPI_IMPL_H

#include "lj_obj.h"

TValue *uj_capi_index2adr(lua_State *L, int idx);

#endif /* !_UJ_CAPI_IMPL_H */
