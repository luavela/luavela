/*
 * uJIT internal unwinder. Provides an API for inspecting VM and Lua stacks
 * in order to find catch frames for run-time exceptions. Also provides an API
 * to perform clean-up of all frames that are destroyed during unwinding
 * stacks towards the catch frame. The API is intended to be used by DWARF 2
 * personality routine (aka external unwinder; see lj_err.c for details).
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJ_UNWIND_H
#define _UJ_UNWIND_H

#include "lj_def.h"

struct lua_State;

/*
 * For coroutine `L`, searches a frame capable of catching the exception `ex`.
 * Stack traversal is done not further than the host stack pointer `ext_cframe`
 * points to. If the catch frame is found, a non-NULL value is returned.
 * Otherwise returns NULL.
 */
void *uj_unwind_search(lua_State *L, int ex, const void *ext_cframe);

/*
 * For coroutine `L`, searches a frame capable of catching the exception `ex`.
 * Stack traversal is done not further than the host stack pointer `ext_cframe`
 * points to. During traversal towards `ext_cframe`, all Lua frames are cleaned
 * up. If the catch frame is reached, a non-NULL value is returned.
 * Otherwise returns NULL.
 * NB! If an error function was called before raising `ex`, all its return
 * values are preserved and moved to the new Lua stack top.
 */
void *uj_unwind_cleanup(lua_State *L, int ex, const void *ext_cframe);

#endif /* !_UJ_UNWIND_H */
