/*
 * uJIT signal-based timers.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "uj_arch.h"
#include "uj_sigtimer.h"

#ifdef UJIT_TIMER

#include <time.h>

#if UJ_TARGET_LINUX
#include <unistd.h>
#include <sys/syscall.h>
#else
#error "Please port Linux-specific features: SA_SIGINFO, SIGEV_THREAD_ID"
/* Notes on usage of Linux-specific features and porting hints:
 * 1. SA_SIGINFO: Allows to use a 3-argument signal handler, which in turn lets
 *    us retrieve a number of timer overruns without an extra syscall.
 *    POSIX porting hint: get rid of SA_SIGINFO, stick to 1-argument signal
 *    handler and call timer_getoverrun from it (it is async-signal-safe).
 * 2. SIGEV_THREAD_ID: Allows to specify which thread should handle the signal
 *    without caring about per-thread signal mask.
 *    POSIX porting hint: First block your profiling signal in the main thread
 *    with pthread_sigmask (make sure you do this before any other thread is
 *    created). This will ensure that all new threads will be created with
 *    the signal blocked. When starting profiling a certain thread, unblock the
 *    signal for it (pthread_sigmask again). When profiling is stopped, do not
 *    forget to block the signal.
 */
#endif

static enum sigtimer_status sigtimer_install_sa(struct sigtimer *timer)
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction));

	sa.sa_flags = SA_RESTART | SA_SIGINFO;
	sa.sa_sigaction = timer->opt.callback;
	if (sigemptyset(&sa.sa_mask) != 0)
		return SIGTIMER_ERR;

	return sigaction(timer->opt.signo, &sa, &timer->old_sa) == 0 ?
		       SIGTIMER_SUCCESS :
		       SIGTIMER_ERR;
}

static enum sigtimer_status sigtimer_uninstall_sa(const struct sigtimer *timer)
{
	return sigaction(timer->opt.signo, &timer->old_sa, NULL) == 0 ?
		       SIGTIMER_SUCCESS :
		       SIGTIMER_ERR;
}

static enum sigtimer_status sigtimer_create(struct sigtimer *timer)
{
	struct sigevent se;
	memset(&se, 0, sizeof(struct sigevent));

	se.sigev_signo = timer->opt.signo;
	se.sigev_notify = SIGEV_THREAD_ID;
	se._sigev_un._tid = syscall(SYS_gettid);

	return timer_create(CLOCK_MONOTONIC, &se, &(timer->id)) == 0 ?
		       SIGTIMER_SUCCESS :
		       SIGTIMER_ERR;
}

/* Low-level timer removal. */
static enum sigtimer_status sigtimer_delete(const struct sigtimer *timer)
{
	return timer_delete(timer->id) == 0 ? SIGTIMER_SUCCESS : SIGTIMER_ERR;
}

static enum sigtimer_status sigtimer_start(const struct sigtimer *timer)
{
	const time_t sec = (time_t)(timer->opt.usec / 1000000U);
	const long nsec = ((long)timer->opt.usec) * 1000U;
	struct itimerspec tm;

	memset(&tm, 0, sizeof(struct itimerspec));

	tm.it_interval.tv_sec = tm.it_value.tv_sec = sec;
	tm.it_interval.tv_nsec = tm.it_value.tv_nsec = nsec;

	return timer_settime(timer->id, 0, &tm, NULL) == 0 ? SIGTIMER_SUCCESS :
							     SIGTIMER_ERR;
}

/* Low-level timer disarming. Currently async-signal-safe. */
static enum sigtimer_status sigtimer_stop(const struct sigtimer *timer)
{
	struct itimerspec tm;

	memset(&tm, 0, sizeof(struct itimerspec));
	return timer_settime(timer->id, 0, &tm, NULL) == 0 ? SIGTIMER_SUCCESS :
							     SIGTIMER_ERR;
}

/* Public API */

enum sigtimer_status uj_sigtimer_init(struct sigtimer *timer,
				      const struct sigtimer_opt *opt)
{
	memcpy(&(timer->opt), opt, sizeof(struct sigtimer_opt));
	return sigtimer_install_sa(timer);
}

enum sigtimer_status uj_sigtimer_start(struct sigtimer *timer)
{
	if (sigtimer_create(timer) != SIGTIMER_SUCCESS)
		return SIGTIMER_ERR;

	if (sigtimer_start(timer) != SIGTIMER_SUCCESS)
		return SIGTIMER_ERR;

	return SIGTIMER_SUCCESS;
}

enum sigtimer_status uj_sigtimer_stop(const struct sigtimer *timer)
{
	if (sigtimer_stop(timer) != SIGTIMER_SUCCESS)
		return SIGTIMER_ERR;

	if (sigtimer_delete(timer) != SIGTIMER_SUCCESS)
		return SIGTIMER_ERR;

	return SIGTIMER_SUCCESS;
}

enum sigtimer_status uj_sigtimer_terminate(const struct sigtimer *timer)
{
	return sigtimer_uninstall_sa(timer);
}

#else /* UJIT_SIGTIMER */

enum sigtimer_status uj_sigtimer_init(struct sigtimer *timer,
				      const struct sigtimer_opt *opt)
{
	UNUSED(timer);
	UNUSED(opt);
	return SIGTIMER_ERR;
}

enum sigtimer_status uj_sigtimer_start(struct sigtimer *timer)
{
	UNUSED(timer);
	return SIGTIMER_ERR;
}

enum sigtimer_status uj_sigtimer_stop(const struct sigtimer *timer)
{
	UNUSED(timer);
	return SIGTIMER_ERR;
}

enum sigtimer_status uj_sigtimer_terminate(const struct sigtimer *timer)
{
	UNUSED(timer);
	return SIGTIMER_ERR;
}

#endif /* UJIT_TIMER */
