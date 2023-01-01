/*
 * uJIT platform-level profiler.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJIT_PROFILE_IFACE_H
#define _UJIT_PROFILE_IFACE_H

#include "lextlib.h"
#include "uj_vmstate.h"

#define VDSO_NAME "vdso"

/*
 * Forward declarations.
 */
struct lua_State;
struct global_State;

enum profiler_mode {
	PROFILE_DEFAULT, /* in-memory per VM state counters
			    * (cheap; always collected)
			    */
	PROFILE_LEAF, /* + streaming leaf profile */
	PROFILE_CALLGRAPH /* + streaming callgraph */
};

/* Options accepted by the profiler. */
struct profiler_options {
	uint32_t interval; /* Profiling interval, in microseconds aka usec
			      (1 sec = 1000000 usec) */
	enum profiler_mode mode;
};

/*
 * Number of distinct VM states, as seen by the profiler during
 * per-VM state profiling. See uj_vmstate.h for more details.
 */
#define UJ_PROFILE_NUM_DISTINCT_VM_STATES (UJ_VMST__MAX + 1)

/* Counters reported back by the profiler. */
struct profiler_data {
	uint64_t id; /* profile ID (e.g. for profiles from various
				  VMs in multithreaded env) */
	uint64_t num_samples; /* Number of times profiling signal handler
				  was actually invoked */
	uint64_t num_overruns; /* Number of signal overruns detected during
				  num_samples invocations */
	uint64_t vmstate[UJ_PROFILE_NUM_DISTINCT_VM_STATES]; /* Counters for
								VM states */
};

/*
 * Returns LUAE_PROFILE_SUCCESS if profiling is available and LUAE_PROFILE_ERR
 * otherwise.
 */
int uj_profile_available(void);

/*
 * Global profiler initialization. Returns LUAE_PROFILE_SUCCESS on success,
 * LUAE_PROFILE_ERR otherwise (e.g. initialization is already performed).
 */
int uj_profile_init(void);

/*
 * Global profiler termination. Termination is performed only if
 * the profiler was initialized and is in a non-running state at the time of
 * the call. Returns LUAE_PROFILE_SUCCESS on success, LUAE_PROFILE_ERR otherwise.
 */
int uj_profile_terminate(void);

/*
 * Starts profiling VM which executes coroutine L. Returns LUAE_PROFILE_SUCCESS
 * if profiling was successfully started. Otherwise (e.g. global initialization
 * was not performed or another VM is already being profiled in a multi-threaded
 * environment) does nothing and returns LUAE_PROFILE_ERR. Please note that only
 * one VM can be profiled at a time.
 */
int uj_profile_start(struct lua_State *L, const struct profiler_options *opt,
		     int fd);

/*
 * In case the profiler is running, reports profiling results by copying already
 * collected counters to out and returns LUAE_PROFILE_SUCCESS. Otherwise does
 * nothing (out is left intact) and returns LUAE_PROFILE_ERR.
 */
int uj_profile_report(struct profiler_data *out);

/*
 * Stops the profiler. This function can be called from any thread.
 * Returns LUAE_PROFILE_SUCCESS if stop was successful or if profiling was
 * already stopped. In case of error returns one of:
 *
 *  * LUAE_PROFILE_ERRMEM: Error freeing output buffer's memory
 *  * LUAE_PROFILE_ERRIO:  I/O error during streaming profiling events
 *
 * Please note that the function blocks untill stop is complete.
 */
int uj_profile_stop(void);

/*
 * If VM g is currently being profiled, behaves exactly as uj_profile_stop().
 * Otherwise does nothing and returns LUAE_PROFILE_ERR. This interface should be
 * used when g is about to be destroyed to finalize profiling gracefully.
 */
int uj_profile_stop_vm(const struct global_State *g);

#endif /* !_UJIT_PROFILE_IFACE_H */
