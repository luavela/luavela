/*
 * Interfaces for dumping loaded shared objects info.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJ_PROFILE_SO_H
#define _UJ_PROFILE_SO_H

#include "lj_def.h"

struct profiler_state;
struct lua_State;

/* Fills array of the loaded shared objects */
int uj_profile_so_init(struct lua_State *L, struct profiler_state *ps);

/* Free all allocated memory */
void uj_profile_so_free(struct profiler_state *ps);

/* Get current instruction pinter and save into struct profiler_state */
void uj_profile_so_get_rip(struct profiler_state *ps, const void *context);

#endif /* !_UJ_PROFILE_SO_H */
