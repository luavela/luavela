/*
 * uJIT instrumenting profiler
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJIT_IPROF_IFACE_H
#define _UJIT_IPROF_IFACE_H

struct lua_State;
struct GCtab;

enum iprof_mode { IPROF_PLAIN, IPROF_BADMODE };

#ifdef UJIT_IPROF_ENABLED

#include <time.h>
#include "lj_def.h"

union GCfunc;

enum iprof_node_type {
	IPROF_START,
	IPROF_RESUME,
	IPROF_DUMMY,
	IPROF_YIELD,
	IPROF_STOP
};

struct iprof_node {
	union {
		union GCfunc *function;
		const char *name;
	};
	struct timespec time;
	enum iprof_node_type type;
	enum iprof_mode mode;
};

struct iprof {
	struct iprof_node *tape;
	size_t size;
	size_t capacity;
	enum iprof_mode mode;
	uint32_t profiling;
};

void uj_iprof_tick(struct lua_State *L, enum iprof_node_type type);
void uj_iprof_tfree(struct lua_State *L);

#endif /* UJIT_IRPOF_ENABLED */

enum iprof_status { IPROF_SUCCESS, IPROF_ERRMEM, IPROF_ERRNYI, IPROF_ERRERR };

enum iprof_status uj_iprof_start(struct lua_State *L, const char *name,
				 enum iprof_mode mode);
enum iprof_status uj_iprof_stop(struct lua_State *L, struct GCtab **result);

#endif /* !_UJIT_IPROF_IFACE_H */
