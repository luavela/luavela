/*
 * uJIT platform-level sampling profiler.
 * General idea is simple: When profiling is started, we arm a timer and bump
 * certain counters each time timer event fires. A dedicated reporting interface
 * can be used to obtain current values of counters.
 *
 * In multi-threaded environment, several VMs can co-exist in multiple threads,
 * but only one VM per process can be profiled at a time: Only this VM receives
 * and handles profiler events. Profiler events block thread execution.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "lj_obj.h"
#include "profile/uj_profile_iface.h"

#ifdef UJIT_PROFILER

#include "jit/lj_jit.h"
#include "profile/uj_profile_impl.h"
#include "profile/uj_profile_so.h"

/*
 * While streaming symbol names, they are marked with the profcount tag and are
 * actually written only at the first occurence. This tag is not reset between
 * profiling sessions to avoid traversing GCobj's: instead, this traversal is
 * done when VM's profcount reaches MAX_PROFCOUNT. All in all, decrease this
 * parameter to save more space and increase it to minimize number of GCobj obj
 * traversals. (blah_MAX - 1) in static assetions below comes from comparison
 * against (MAX_PROFCOUNT + 1) in the code.
 */
#define MAX_PROFCOUNT 254

LJ_STATIC_ASSERT(sizeof(((global_State *)0)->profcount) == 1);
LJ_STATIC_ASSERT(MAX_PROFCOUNT >= 1 && MAX_PROFCOUNT <= UINT8_MAX - 1);

/* Signal to be used to notify about profiling events */
#define PROFILING_SIGNAL SIGALRM

#ifdef UJIT_IS_THREAD_SAFE

pthread_mutex_t profiler_state_mutex = PTHREAD_MUTEX_INITIALIZER;

#define LOCK_PROFILER_STATE() pthread_mutex_lock(&profiler_state_mutex)
#define UNLOCK_PROFILER_STATE() pthread_mutex_unlock(&profiler_state_mutex)

#else /* UJIT_IS_THREAD_SAFE */

#define LOCK_PROFILER_STATE()
#define UNLOCK_PROFILER_STATE()

#endif /* UJIT_IS_THREAD_SAFE */

/*
 * NB! As we use static profiler state, only one VM
 * (global_State in lj_obj terms) can be profiled at a time.
 */
static struct profiler_state profiler_state;

static LJ_AINLINE int profile_is_init(const struct profiler_state *ps)
{
	return ps->flags & STATE_INITIALIZED;
}

/*
 * Traverses the *entire* chain of collectable objects and sets profcount to 0
 * for relevant objects. Obviously slow, should be avoided (see MAX_PROFCOUNT).
 */
static void profile_reset_gco_profcounts(global_State *g)
{
	GCobj *o;
	GCobj **iter = &g->gc.root;

	while (NULL != (o = *iter)) {
		switch (o->gch.gct) {
		case (~LJ_TPROTO):
			gco2pt(o)->profcount = 0;
			break;
		case (~LJ_TTRACE):
			gco2trace(o)->profcount = 0;
			break;
		default:
			break;
		}
		iter = &o->gch.nextgc;
	}
}

/* ---------------------- Main profiler payload ----------------------------- */

/* Increments in-memory VM state counters and streams data about the event. */
static void profile_handle_event(struct profiler_state *ps)
{
	global_State *g = ps->g;
	uint32_t _vmstate = ~(uint32_t)(g->vmstate);
	uint32_t vmstate = _vmstate < UJ_VMST_TRACE ? _vmstate : UJ_VMST_TRACE;

	ps->data.vmstate[vmstate]++;

	uj_profile_stream_event(ps, vmstate);
}

/* Profiling signal handler. */
static void profile_signal_handler(int sig, siginfo_t *si, void *context)
{
	struct profiler_state *ps = &profiler_state;
	UNUSED(sig);

	uj_profile_so_get_rip(ps, context);

	switch (ps->state) {
	case PS_IDLE:
		return;
	case PS_PROFILE:
		/*
		 * The only thread that can handle the signal during profiling
		 * is the one which started the profiler. pthread_self()
		 * is async-signal-safe.
		 */
		lua_assert(pthread_self() == ps->thread);

		/* Do actual profiling job */
		ps->data.num_samples++;
		ps->data.num_overruns += si->si_overrun;

		profile_handle_event(ps);
		if (LJ_LIKELY(!ujp_write_test_flag(&ps->buf, STREAM_ERR_IO)))
			return;

		/*
		 * Error during streaming -> no more payload handling (but
		 * timer and signal handling are still active - to be cleaned
		 * properly on stop).
		 */
		ps->state = PS_ERROR;
		return;
	case PS_ERROR:
		return;
	default:
		return;
	}
}

/* ---------------------- Implementation of public API ---------------------- */

int uj_profile_available(void)
{
	return LUAE_PROFILE_SUCCESS;
}

int uj_profile_init(void)
{
	struct profiler_state *ps = &profiler_state;
	int status;

	LOCK_PROFILER_STATE();

	if (!profile_is_init(ps)) {
		struct sigtimer_opt opt = {.signo = PROFILING_SIGNAL,
					   .usec = 0,
					   .callback = profile_signal_handler};

		if (uj_sigtimer_init(&ps->timer, &opt) == SIGTIMER_SUCCESS) {
			ps->flags |= STATE_INITIALIZED;
			status = LUAE_PROFILE_SUCCESS;
		} else {
			status = LUAE_PROFILE_ERR;
		}
	} else {
		status = LUAE_PROFILE_ERR;
	}

	UNLOCK_PROFILER_STATE();
	return status;
}

int uj_profile_terminate(void)
{
	struct profiler_state *ps = &profiler_state;
	int status;

	LOCK_PROFILER_STATE();

	if (profile_is_init(ps) && ps->state == PS_IDLE) {
		if (uj_sigtimer_terminate(&ps->timer) == SIGTIMER_SUCCESS) {
			memset(ps, 0, sizeof(*ps));
			ps->state = PS_IDLE;
			status = LUAE_PROFILE_SUCCESS;
		} else {
			status = LUAE_PROFILE_ERR;
		}
	} else {
		status = LUAE_PROFILE_ERR;
	}

	UNLOCK_PROFILER_STATE();
	return status;
}

static int profile_start_validate(const struct profiler_state *ps,
				  const struct profiler_options *opt)
{
	if (!profile_is_init(ps))
		return LUAE_PROFILE_ERR;
	if (ps->state != PS_IDLE)
		return LUAE_PROFILE_ERR;
	if (opt->interval == 0)
		return LUAE_PROFILE_ERR;
	return LUAE_PROFILE_SUCCESS;
}

void uj_profile_start_init(struct profiler_state *ps, lua_State *L,
			   const struct profiler_options *opt)
{
	global_State *g = G(L);

	/* Initialize relevant top-level fields: */
	ps->g = g;
	ps->thread = pthread_self();

	/* Initialize counters: */
	memset(&ps->data, 0, sizeof(ps->data));
	ps->data.id = (uint64_t)g;

	/* Initialize profiler options: */
	memcpy(&ps->opt, opt, sizeof(ps->opt));

	/* Initialize timer options: */
	ps->timer.opt.usec = opt->interval;

	g->profcount++;
	if (g->profcount == MAX_PROFCOUNT + 1) {
		profile_reset_gco_profcounts(g);
		g->profcount = 1;
	}
}

int uj_profile_start(lua_State *L, const struct profiler_options *opt, int fd)
{
	struct profiler_state *ps = &profiler_state;
	int stream_status;

	LOCK_PROFILER_STATE();

	if (profile_start_validate(ps, opt) != LUAE_PROFILE_SUCCESS) {
		UNLOCK_PROFILER_STATE();
		return LUAE_PROFILE_ERR;
	}

	uj_profile_start_init(ps, L, opt);

	if (uj_profile_so_init(L, ps) != 0) {
		UNLOCK_PROFILER_STATE();
		return LUAE_PROFILE_ERR;
	}

	stream_status = uj_profile_stream_start(ps, L, fd);
	if (stream_status != LUAE_PROFILE_SUCCESS) {
		UNLOCK_PROFILER_STATE();
		return stream_status;
	}

	if (uj_sigtimer_start(&ps->timer) != SIGTIMER_SUCCESS) {
		UNLOCK_PROFILER_STATE();
		return LUAE_PROFILE_ERR;
	}

	lua_assert(PS_IDLE == ps->state);
	ps->state = PS_PROFILE;
	UNLOCK_PROFILER_STATE();
	return LUAE_PROFILE_SUCCESS;
}

int uj_profile_report(struct profiler_data *out)
{
	const struct profiler_state *ps = &profiler_state;
	int status;

	LOCK_PROFILER_STATE();

	if (ps->state == PS_PROFILE) {
		memcpy(out, &ps->data, sizeof(ps->data));
		status = LUAE_PROFILE_SUCCESS;
	} else {
		status = LUAE_PROFILE_ERR;
	}

	UNLOCK_PROFILER_STATE();
	return status;
}

/*
 * NB! Stopping interfaces can be invoked from any thread.
 * Caller moves profiler to the idle state which effectively discards
 * profiling event processing, and disarms+deletes timer after that.
 */

static int profile_stop(const global_State *g)
{
	struct profiler_state *ps = &profiler_state;
	int status = LUAE_PROFILE_SUCCESS;

	LOCK_PROFILER_STATE();

	if (g != NULL && ps->g != g) {
		UNLOCK_PROFILER_STATE();
		return LUAE_PROFILE_ERR;
	}

	if (ps->state != PS_IDLE) {
		/*
		 * Stop handling profiling events and cleanup all aux stuff.
		 * For simplicity, report the last encountered error. Fix this
		 * when it becomes a problem.
		 */
		int stream_status;
		lua_assert(ps->state == PS_PROFILE || ps->state == PS_ERROR);

		ps->state = PS_IDLE;

		if (uj_sigtimer_stop(&ps->timer) != SIGTIMER_SUCCESS)
			status = LUAE_PROFILE_ERR;

		stream_status = uj_profile_stream_stop(ps);
		if (stream_status != LUAE_PROFILE_SUCCESS)
			status = stream_status;
	}

	/* Clear all dumped libs */
	uj_profile_so_free(ps);

	UNLOCK_PROFILER_STATE();
	return status;
}

int uj_profile_stop(void)
{
	return profile_stop(NULL);
}

int uj_profile_stop_vm(const global_State *g)
{
	return profile_stop(g);
}

#else /* !UJIT_PROFILER */

/* ---------------------- Public API stubs ---------------------------------- */

int uj_profile_available(void)
{
	return LUAE_PROFILE_ERR;
}

int uj_profile_init(void)
{
	return LUAE_PROFILE_ERR;
}

int uj_profile_terminate(void)
{
	return LUAE_PROFILE_ERR;
}

int uj_profile_start(lua_State *L, const struct profiler_options *opt, int fd)
{
	UNUSED(L);
	UNUSED(opt);
	UNUSED(fd);
	return LUAE_PROFILE_ERR;
}

int uj_profile_report(struct profiler_data *out)
{
	UNUSED(out);
	return LUAE_PROFILE_ERR;
}

int uj_profile_stop(void)
{
	return LUAE_PROFILE_ERR;
}

int uj_profile_stop_vm(const global_State *g)
{
	UNUSED(g);
	return LUAE_PROFILE_ERR;
}

#endif /* UJIT_PROFILER */
