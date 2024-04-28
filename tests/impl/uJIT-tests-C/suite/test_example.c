/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common.h"

/*
 * NB! To add the unit test to the suite,
 * please edit CMakeLists.txt in the current dir.
 *
 * Translation units to be tested should be included below. Please note: for
 * unit testing, it is preferred to include *translation units* (*.c files),
 * not headers. For convenience uJIT src directory is already added to
 * -I in the cmake, so use something like:
 */
/* #include <utils/lj_strscan.c> */

/*
 * Full list of cmocka assertions can be found here:
 * https://api.cmocka.org/group__cmocka__asserts.html
 *
 * Full documentation for cmocka can be found here:
 * https://api.cmocka.org/modules.html
 */

static void test_feature1(void **state) /* This is a single case */
{
	UNUSED_STATE(state); /* Needed for smarter control of test execution */

	assert_true(1);
	assert_string_equal("42", "42");
	assert_string_not_equal("sea", "see");
}

static void test_feature2(void **state)
{
	UNUSED_STATE(state);

	int got = 1;
	int expected = 1;
	void *buffer = NULL;

	assert_false(-1 + 1);
	assert_int_equal(got, expected);

	assert_null(buffer);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_feature1),
		cmocka_unit_test(test_feature2),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
