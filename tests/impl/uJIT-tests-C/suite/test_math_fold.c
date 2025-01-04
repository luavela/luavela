/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common.h"
#include <utils/uj_math.h>

#define PI 3.14159265

/* The purpose of this test is to ensure that different calculations are
 * not messed up. We're not testing particular implementations of addition,
 * power or whatever else.
 * If particular operation yields the result that looks like the result of this
 * operation, and the output of other operations would be significantly
 * different on the same input, we're fine.
 */
static void test_foldarith(void **state)
{
	UNUSED_STATE(state);

	/* ADD */
	assert_double_equal(5., uj_math_foldarith(1., 4., FoldarithAdd), 1e-6);

	/* SUB */
	assert_double_equal(5., uj_math_foldarith(10., 5., FoldarithSub), 1e-6);

	/* MUL */
	assert_double_equal(28., uj_math_foldarith(4., 7., FoldarithMul), 1e-6);

	/* DIV */
	assert_double_equal(4., uj_math_foldarith(28., 7., FoldarithDiv), 1e-6);

	/* MOD */
	assert_double_equal(2., uj_math_foldarith(14., 6., FoldarithMod), 1e-6);

	/* POW */
	assert_double_equal(9., uj_math_foldarith(3., 2., FoldarithPow), 1e-6);

	/* NEG */
	assert_double_equal(4., uj_math_foldarith(-4., 12345., FoldarithNeg),
			    1e-6);
	assert_double_equal(-4., uj_math_foldarith(4., 12345., FoldarithNeg),
			    1e-6);

	/* ABS */
	assert_double_equal(4., uj_math_foldarith(4., 54321., FoldarithAbs),
			    1e-6);
	assert_double_equal(4., uj_math_foldarith(-4., 54321., FoldarithAbs),
			    1e-6);

	/* ATAN2 */
	assert_double_equal(
		135., 180. / PI * uj_math_foldarith(7., -7., FoldarithAtan2),
		1e-6);

	/* LDEXP */
	assert_double_equal(15.2, uj_math_foldarith(0.95, 4., FoldarithLdexp),
			    1e-6);

	/* MIN */
	assert_double_equal(18., uj_math_foldarith(120., 18., FoldarithMin),
			    1e-6);
	assert_double_equal(18., uj_math_foldarith(18., 120., FoldarithMin),
			    1e-6);

	/* MAX */
	assert_double_equal(120., uj_math_foldarith(120., 18., FoldarithMax),
			    1e-6);
	assert_double_equal(120., uj_math_foldarith(18., 120., FoldarithMax),
			    1e-6);
}

/* The purpose of this test is to ensure that different calculations are
 * not messed up. We're not testing particular implementations of addition,
 * power or whatever else.
 * If particular operation yields the result that looks like the result of this
 * operation, and the output of other operations would be significantly
 * different on the same input, we're fine.
 */
static void test_foldfpm(void **state)
{
	UNUSED_STATE(state);

	/* FLOOR */
	assert_double_equal(5., uj_math_foldfpm(5.2, FoldfpmFloor), 1e-6);
	assert_double_equal(-6., uj_math_foldfpm(-5.2, FoldfpmFloor), 1e-6);

	/* CEIL */
	assert_double_equal(6., uj_math_foldfpm(5.2, FoldfpmCeil), 1e-6);
	assert_double_equal(-5., uj_math_foldfpm(-5.2, FoldfpmCeil), 1e-6);

	/* TRUNC */
	assert_double_equal(5., uj_math_foldfpm(5.2, FoldfpmTrunc), 1e-6);
	assert_double_equal(-5., uj_math_foldfpm(-5.2, FoldfpmTrunc), 1e-6);

	/* SQRT */
	assert_double_equal(4., uj_math_foldfpm(16., FoldfpmSqrt), 1e-6);

	/* EXP */
	assert_double_equal(7.389056, uj_math_foldfpm(2., FoldfpmExp), 1e-6);

	/* EXP2 */
	assert_double_equal(0.0625, uj_math_foldfpm(-4., FoldfpmExp2), 1e-6);

	/* LOG */
	assert_double_equal(2.014903, uj_math_foldfpm(7.5, FoldfpmLog), 1e-6);

	/* LOG2 */
	assert_double_equal(16., uj_math_foldfpm(65536., FoldfpmLog2), 1e-6);

	/* LOG10 */
	assert_double_equal(3., uj_math_foldfpm(1000., FoldfpmLog10), 1e-6);

	/* SIN */
	assert_double_equal(0.5, uj_math_foldfpm(PI / 6, FoldfpmSin), 1e-6);

	/* COS */
	assert_double_equal(0.5, uj_math_foldfpm(PI / 3, FoldfpmCos), 1e-6);

	/* TAN */
	assert_double_equal(1., uj_math_foldfpm(PI / 4., FoldfpmTan), 1e-6);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_foldarith),
		cmocka_unit_test(test_foldfpm),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
