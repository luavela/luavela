/*
 * uJIT timer interrupts. Interrupts are delivered via a configurable signal.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJIT_TIMERINT_H
#define _UJIT_TIMERINT_H

#include <sys/time.h>

#include "lj_def.h"

#define TIMERINT_INTERVAL_USEC ((uint64_t)(1000)) /* = 1ms = 0.001s */

LJ_STATIC_ASSERT(TIMERINT_INTERVAL_USEC > 0);

/*
 * Initializes interrupts. Interrupts are delivered to the application via
 * the signo signal. Returns LUAE_INT_SUCCESS on success, LUAE_INT_ERR otherwise.
 * An attempt to initialize interrupts more than once in a row is an error.
 */
int uj_timerint_init(int signo);

/*
 * Initializes interrupts. Interrupts are delivered to the application via
 * some implementation-defined signal. Returns LUAE_INT_SUCCESS on success,
 * LUAE_INT_ERR otherwise. An attempt to default-initialize already initialized
 * interrupts is not an error. Intended to be used from inside the platform.
 */
int uj_timerint_init_default(void);

/*
 * Terminates interrupts. Returns LUAE_INT_SUCCESS on success, LUAE_INT_ERR
 * otherwise. An attempt to terminate interrupts more than once in a row is
 * an error.
 */
int uj_timerint_terminate(void);

/*
 * Terminates interrupts initialized via uj_timerint_init_default().
 * Returns LUAE_INT_SUCCESS on success, LUAE_INT_ERR otherwise.
 * An attempt to default-terminate interrupts more than once in a row is not an
 * error. Intended to be used from inside the platform.
 */
int uj_timerint_terminate_default(void);

/* Returns a non-zero value if interrupts are initialized, and 0 otherwise. */
int uj_timerint_is_ticking(void);

/*
 * If interrupts are initialized, returns the number of ticks since
 * initialization. Otherwise the return value is undefined.
 */
uint64_t uj_timerint_ticks(void);

/*
 * Checks if the value specified by time is valid. All structure members must
 * be non-negative to comply. Returns a non-0 value on success, and 0 otherwise.
 */
static LJ_AINLINE int uj_timerint_is_valid(const struct timeval *time)
{
	return time->tv_sec >= 0 && time->tv_usec >= 0;
}

/*
 * Converts a time value to microseconds. If either field of the struct timeval
 * is negative, returns 0.
 */
static LJ_AINLINE uint64_t uj_timerint_to_usec(const struct timeval *time)
{
	lua_assert(uj_timerint_is_valid(time));
	return (uint64_t)(time->tv_sec * 1E6 + time->tv_usec);
}

/* Converts a time value in microseconds to ticks. */
static LJ_AINLINE uint64_t uj_timerint_to_ticks(uint64_t usec)
{
	return usec / TIMERINT_INTERVAL_USEC;
}

/*
 * Returns a non-0 value if a value in microseconds has resolution greater than
 * or equal to the one provided by the timer interrupts. Otherwise returns 0.
 */
static LJ_AINLINE int uj_timerint_is_resolvable_usec(uint64_t usec)
{
	return usec >= TIMERINT_INTERVAL_USEC;
}

#endif /* !_UJIT_TIMERINT_H */
