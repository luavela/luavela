/*
 * uJIT timer interrupts.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "uj_timerint.h"
#include "lextlib.h"

#ifdef UJIT_TIMER

#include <signal.h>

#include "lj_def.h"
#include "uj_sigtimer.h"

#define TIMERINT_DEFAULT_SIGNO SIGPROF

#define TIMERINT_IS_TICKING (0x1)
#define TIMERINT_DEFAULT_INIT (0x2)

struct timerint {
	uint64_t ticks;
	uint64_t flags;
	struct sigtimer timer;
};

static struct timerint timerint;

static LJ_NOINLINE void timerint_handler(int sig, siginfo_t *si, void *context)
{
	UNUSED(sig);
	UNUSED(si);
	UNUSED(context);
	(timerint.ticks)++;
}

int uj_timerint_is_ticking(void)
{
	return timerint.flags & TIMERINT_IS_TICKING;
}

int uj_timerint_init(int signo)
{
	struct sigtimer *timer = &(timerint.timer);
	const struct sigtimer_opt opt = {.signo = signo,
					 .usec = TIMERINT_INTERVAL_USEC,
					 .callback = timerint_handler};

	if (uj_timerint_is_ticking())
		return LUAE_INT_ERR;

	if (uj_sigtimer_init(timer, &opt) != SIGTIMER_SUCCESS)
		return LUAE_INT_ERR;

	if (uj_sigtimer_start(timer) != SIGTIMER_SUCCESS)
		return LUAE_INT_ERR;

	timerint.ticks = 0;
	timerint.flags |= TIMERINT_IS_TICKING;

	return LUAE_INT_SUCCESS;
}

int uj_timerint_init_default(void)
{
	int status;

	if (uj_timerint_is_ticking())
		return LUAE_INT_SUCCESS;

	status = uj_timerint_init(TIMERINT_DEFAULT_SIGNO);

	if (LUAE_INT_SUCCESS == status)
		timerint.flags |= TIMERINT_DEFAULT_INIT;

	return status;
}

int uj_timerint_terminate(void)
{
	const struct sigtimer *timer = &(timerint.timer);

	if (!uj_timerint_is_ticking())
		return LUAE_INT_ERR;

	if (uj_sigtimer_stop(timer) != SIGTIMER_SUCCESS)
		return LUAE_INT_ERR;

	if (uj_sigtimer_terminate(timer) != SIGTIMER_SUCCESS)
		return LUAE_INT_ERR;

	timerint.flags &= ~(TIMERINT_IS_TICKING);

	return LUAE_INT_SUCCESS;
}

int uj_timerint_terminate_default(void)
{
	int status;

	if (!(timerint.flags & TIMERINT_DEFAULT_INIT))
		return LUAE_INT_SUCCESS;

	status = uj_timerint_terminate();

	if (LUAE_INT_SUCCESS == status)
		timerint.flags &= ~(TIMERINT_DEFAULT_INIT);

	return status;
}

uint64_t uj_timerint_ticks(void)
{
	return timerint.ticks;
}

#else /* UJIT_TIMER */

int uj_timerint_init(int signo)
{
	UNUSED(signo);
	return LUA_INT_ERR;
}

int uj_timerint_init_default(void)
{
	return LUA_INT_ERR;
}

int uj_timerint_terminate(void)
{
	return LUA_INT_ERR;
}

int uj_timerint_terminate_default(void)
{
	return LUA_INT_ERR;
}

int uj_timerint_is_ticking(void)
{
	return LUA_INT_ERR;
}
uint64_t uj_timerint_ticks(void)
{
	return 0;
}

#endif /* UJIT_TIMER */
