/*
 * Private data structure definitions for uJIT profiler.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJIT_PROFILE_IFACE_PRIVATE_H
#define _UJIT_PROFILE_IFACE_PRIVATE_H

#include <pthread.h>

#include "lj_obj.h"
#include "uj_sigtimer.h"
#include "profile/uj_profile_iface.h"
#include "profile/ujp_write.h"

enum profiling_state {
	PS_IDLE, /* No VM is under profiling. */
	PS_PROFILE, /* Profiling a certain VM. */
	PS_ERROR /* Error encountered during profiling. */
};

#define SO_MAX_PATH_LENGTH 256

struct shared_obj {
	uintptr_t base; /* Shared object base address */
	char path[SO_MAX_PATH_LENGTH]; /* Absolute path to SO */
};

struct so_info {
	struct shared_obj *objects; /* Linear array that holds SO info */
	size_t num; /* Number of shared objects */
};

struct sig_context {
	uint64_t rip;
};

struct profiler_state {
	global_State *g; /* VM which is profiled at the moment. */
	volatile sig_atomic_t state; /* Internal state. */
	struct profiler_data data; /* Profiler counters. */
	struct profiler_options opt; /* Options specified when profiling
				      * was started. */
	pthread_t thread; /* Host thread ID corresponding to
					profiled VM. */
	struct sig_context context; /* Context after signal was occurred */
	struct so_info *so; /* Holds info about all shared objects */
	struct sigtimer timer;
	struct ujp_buffer buf;
	volatile uint8_t flags; /* Internal profiler flags. */
};

/* Profiler flags */
#define STATE_INITIALIZED 0x1

/* Interfaces below should not be exposed outside profiler's infrastructure */

/* Various internal initialization at profiler startup. */
void uj_profile_start_init(struct profiler_state *ps, lua_State *L,
			   const struct profiler_options *opt);

/*
 * Starts streaming events to the output descriptor currently associated
 * with ps. Output descriptor should be prepared by the code that invokes the
 * profiler and passed appropriately via options when profiling session is
 * started. Returns LUAE_PROFILE_SUCCESS on success. In case of error
 * returns one of:
 *
 *  - LUAE_PROFILE_ERRMEM: Error allocating memory for output buffer
 *  - LUAE_PROFILE_ERRIO:  Error writing prologue data
 *
 * In case of any error, there will be an attempt to *close* the output
 * descriptor associated with ps as well as free all other allocated resources,
 * so there is no need to call stream_stop in case stream_start failed.
 */
int uj_profile_stream_start(struct profiler_state *ps, lua_State *L, int fd);

/*
 * Streams a single event inside a profiling signal with VM being in vmstate.
 * Guarantees that only async-signal-safe library functions and syscalls will
 * be used during the call. Returns nothing, but updates internal ps flags
 * to ensure delayed error handling.
 */
void uj_profile_stream_event(struct profiler_state *ps, uint32_t vmstate);

/*
 * Stops streaming events to the output descriptor currently associated
 * with ps. Please note that output descriptor is *closed* during the call.
 * Returns nothing, but updates internal ps flags to ensure delayed error
 * handling.
 */
int uj_profile_stream_stop(struct profiler_state *ps);

#endif /* !_UJIT_PROFILE_IFACE_PRIVATE_H */
