/*
 * Memory profiler.
 *
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJ_MEMPROF_IFACE_H
#define _UJ_MEMPROF_IFACE_H

#include <stdint.h>

struct lua_State;
struct global_State;

#define UJM_CURRENT_FORMAT_VERSION 0x02

#define MEMPROF_DURATION_INFINITE (uint64_t)0

struct memprof_options {
	uint64_t dursec; /* Approximate duration of profiling, seconds. */
	int fd; /* File descriptor for writing output. */
};

/*
 * Starts profiling. Returns LUAE_PROFILE_SUCCESS on success and one of
 * LUAE_PROFILE_ERR* codes otherwise.
 */
int uj_memprof_start(struct lua_State *L, const struct memprof_options *opt);

/*
 * Stops profiling. Returns LUAE_PROFILE_SUCCESS on success and one of
 * LUAE_PROFILE_ERR* codes otherwise.
 */
int uj_memprof_stop(void);

/*
 * VM g is currently being profiled, behaves exactly as uj_memprof_stop().
 * Otherwise does nothing and returns LUAE_PROFILE_ERR.
 */
int uj_memprof_stop_vm(const struct global_State *g);

#endif /* !_UJ_MEMPROF_IFACE_H */
