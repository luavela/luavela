/*
 * uJIT signal-based timers: Timer events are delivered to the application
 * via a configurable signal. Signal callback is handled in the thread that
 * started the timer.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJIT_SIGTIMER_H
#define _UJIT_SIGTIMER_H

#include "lj_def.h"

struct sigtimer;
struct sigtimer_opt;

enum sigtimer_status { SIGTIMER_SUCCESS, SIGTIMER_ERR };

#ifdef UJIT_TIMER

#include <signal.h>

struct sigtimer_opt {
	int signo; /* signal to be used for the timer */
	uint32_t usec; /* timer interval, microseconds (10E-6 sec) */
	void (*callback)(int, siginfo_t *, void *);
};

struct sigtimer {
	timer_t id; /* system timer ID */
	struct sigtimer_opt opt; /* timer options */
	struct sigaction old_sa; /* previous signal action context */
};

#endif /* UJIT_TIMER */

enum sigtimer_status uj_sigtimer_init(struct sigtimer *timer,
				      const struct sigtimer_opt *opt);

enum sigtimer_status uj_sigtimer_start(struct sigtimer *timer);

enum sigtimer_status uj_sigtimer_stop(const struct sigtimer *timer);

enum sigtimer_status uj_sigtimer_terminate(const struct sigtimer *timer);

#endif /* !_UJIT_SIGTIMER_H */
