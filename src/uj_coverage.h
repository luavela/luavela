/*
 * Platform-level coverage counting.
 *
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJ_COVERAGE_H
#define _UJ_COVERAGE_H

#include "lextlib.h"

struct lua_State;

#ifdef UJIT_COVERAGE

#include "lj_bc.h"

struct FuncState;

static LJ_AINLINE int uj_coverage_enabled(const struct lua_State *L)
{
	return G(L)->coverage != NULL;
}

void uj_coverage_emit(struct FuncState *fs);

void uj_coverage_stream_line(struct lua_State *L, const BCIns *pc);

#endif /* UJIT_COVERAGE */

void uj_coverage_pause(struct lua_State *L);

void uj_coverage_unpause(struct lua_State *L);

int uj_coverage_start(struct lua_State *L, const char *filename,
		      const char **excludes, size_t num);
int uj_coverage_start_cb(struct lua_State *L, lua_Coveragewriter cb,
			 void *context, const char **excludes, size_t num);

int uj_coverage_stop(struct lua_State *L);

#endif /* !_UJ_COVERAGE_H */
