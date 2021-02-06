/*
 * uJIT instrumenting profiler
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJIT_IPROF_IFACE_H
#define _UJIT_IPROF_IFACE_H

enum iprof_mode {
	IPROF_PLAIN,
	IPROF_INCLUSIVE,
	IPROF_EXCLUSIVE,
	IPROF_BADMODE
};

enum iprof_status { IPROF_SUCCESS, IPROF_ERRERR, IPROF_ERRNYI };

struct lua_State;

#ifdef UJIT_IPROF_ENABLED

#include "lj_def.h"

enum iprof_ev_type {
	IPROF_START,
	IPROF_RESUME,
	IPROF_CALL,
	IPROF_DUMMY,
	IPROF_RETURN,
	IPROF_YIELD,
	IPROF_STOP,
	IPROF_CALLT
};

enum iprof_keys {
	IPROF_KEY_LUA,
	IPROF_KEY_WALL,
	IPROF_KEY_SUBS,
	IPROF_KEY_CALLS,
	IPROF_KEY_MAX
};

void uj_iprof_tick(struct lua_State *L, enum iprof_ev_type type);
void uj_iprof_unwind(struct lua_State *L);
void uj_iprof_release(struct lua_State *L);

void uj_iprof_keys(struct lua_State *L);
void uj_iprof_unkeys(struct lua_State *L);

#endif /* UJIT_IRPOF_ENABLED */

enum iprof_status uj_iprof_start(struct lua_State *L, const char *name,
				 enum iprof_mode mode, unsigned climit);

struct GCtab;

enum iprof_status uj_iprof_stop(struct lua_State *L, struct GCtab **result);

#endif /* !_UJIT_IPROF_IFACE_H */
