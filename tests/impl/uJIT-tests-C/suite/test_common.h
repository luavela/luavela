/*
 * Common includes and definitions for uJIT unit tests.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJIT_UNIT_TESTS_COMMON_H_
#define _UJIT_UNIT_TESTS_COMMON_H_

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

#include <math.h>
/*
 * XXX: cmocka provides <assert_double_equal> helper since 1.1.6 version.
 * Nevertheless, to support all available cmocka versions (at least while
 * transitioning period), <assert_double_equal> helper is defined below, if
 * none is provided by cmocka.
 */
#ifndef assert_double_equal
#define assert_double_equal(x, y, p) \
	assert_true(fabs(((double)(x)) - ((double)(y))) < (p))
#endif

#define UNUSED(x) ((void)(x))
#define UNUSED_STATE(state) UNUSED(state)

#endif /* !_UJIT_UNIT_TESTS_COMMON_H_ */
