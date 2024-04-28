/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <stdio.h>
#include <stdlib.h>

#include "test_common_lua.h"
#include "lj_obj.h"
#include "uj_cstr.h"

static UJ_UNIT_AINLINE void assert_fromint_vs_sprintf(int32_t k)
{
	char buf1[UJ_CSTR_INTBUF + 1]; /* extra byte for ending '\0' */
	char buf2[UJ_CSTR_INTBUF + 1];
	size_t size;

	size = uj_cstr_fromint(buf1, k);
	buf1[size] = '\0';
	sprintf(buf2, "%d", k);

	assert_string_equal(buf1, buf2);
}

static void test_cstr_fromint(void **state)
{
	UNUSED_STATE(state);

	int i;
	/* Test different ranges with approx logarithmic prime number step */
	for (i = -10000; i < 10000; ++i)
		assert_fromint_vs_sprintf(i);
	for (i = 10000; i < 100000000; i += 9973)
		assert_fromint_vs_sprintf(i);
	for (i = 100000000; i < 2000000000; i += 99999787)
		assert_fromint_vs_sprintf(i);
}

static void assert_fromnum(lua_Number n, const char *ref)
{
	char buf[UJ_CSTR_NUMBUF + 1]; /* extra byte for ending '\0' */
	size_t size;

	size = uj_cstr_fromnum(buf, n);
	buf[size] = '\0';

	assert_string_equal(buf, ref);
}

__attribute__((noinline)) /* Otherwise Werror will fail the build */
static lua_Number
zero(void)
{
	return 0.0;
}

static void test_cstr_fromnum(void **state)
{
	UNUSED_STATE(state);

	assert_fromnum(1 / zero(), "inf");
	assert_fromnum(-1 / zero(), "-inf");
	assert_fromnum(1 / zero() - 1 / zero(), "nan");
	assert_fromnum(1 / 2.0, "0.5");
}

static void assert_tonum_vs_strtod(const char *buf)
{
	lua_Number tonum, tod;
	union TValue tv;
	int status;

	status = uj_cstr_tonum(buf, &tonum);
	assert_int_equal(status, 1);
	status = uj_cstr_tonumtv(buf, &tv);
	assert_int_equal(status, 1);
	tod = strtod(buf, NULL);
	if (!isfinite(tonum)) {
		if (isnan(tonum)) {
			assert_true(isnan(tod));
			assert_true(isnan(numV(&tv)));
		} else if (isinf(tonum)) {
			assert_true(tod == tonum);
			assert_true(tonum == numV(&tv));
		} else {
			assert_true(0); /* Unreachable */
		}
	} else {
		assert_true(tonum == numV(&tv));
		assert_true(fabs(tod - tonum) < 1e-15);
	}
}

static void test_cstr_tonum(void **state)
{
	UNUSED_STATE(state);

	char buf[UJ_CSTR_NUMBUF];
	int exp;
	int denom;
	int sign = 1;

	for (denom = 1; denom < 1000; ++denom) {
		for (exp = -301; exp < 301; exp += 3) {
			lua_Number numexp = exp * 1.0016903 + 0.9887765;
			lua_Number n;

			sign *= -1;
			n = sign * (1.0 / denom) * pow(10, numexp);

			snprintf(buf, UJ_CSTR_NUMBUF, "%f", n);
			assert_tonum_vs_strtod(buf);

			snprintf(buf, UJ_CSTR_NUMBUF, "%e", n);
			assert_tonum_vs_strtod(buf);
		}
	}

	assert_tonum_vs_strtod("inf");
	assert_tonum_vs_strtod("-inf");
	assert_tonum_vs_strtod("infinity");
	assert_tonum_vs_strtod("nan");
	assert_tonum_vs_strtod("NAN");
	assert_tonum_vs_strtod("-INF");
}

static void assert_cstr_find_vs_strstr(const char *haystack, char *needle)
{
	const size_t haystacklen = strlen(haystack);
	const size_t needlelen = strlen(needle);
	int k;

	for (k = needlelen; k >= 0; --k) {
		const char *pfind;
		const char *pstr;
		char memo = needle[k];

		needle[k] = '\0';
		pfind = uj_cstr_find(haystack, needle, haystacklen, k);
		pstr = strstr(haystack, needle);
		assert_ptr_equal(pfind, pstr);
		needle[k] = memo;
	}
}

static void test_cstr_find(void **state)
{
	UNUSED_STATE(state);

	int i, j;
	const char haystack[] = "bcabacbabcbacabacababacacab";
	char needle[] = "abcbaca";
	const size_t nlen = strlen(needle);

	for (i = 0; i < strlen(haystack); ++i)
		for (j = 0; j < nlen; ++j)
			assert_cstr_find_vs_strstr(haystack + i, needle + j);

	/* Edge cases: */
	assert_ptr_equal(uj_cstr_find("abc", "d", 3, 1), NULL);
	assert_ptr_equal(uj_cstr_find("abc", "abcd", 3, 4), NULL);

	/* '\0' symbol: */
	assert_ptr_equal(uj_cstr_find("ab\0cd", "abcd", 5, 4), NULL);
	needle[4] = '\0';
	assert_ptr_equal(uj_cstr_find(needle, needle + 2, nlen, nlen - 2),
			 needle + 2);
}

int main(void)
{
	const struct CMUnitTest tests[] = {cmocka_unit_test(test_cstr_fromint),
					   cmocka_unit_test(test_cstr_fromnum),
					   cmocka_unit_test(test_cstr_tonum),
					   cmocka_unit_test(test_cstr_find)};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
