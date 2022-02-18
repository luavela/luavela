/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common.h"
#include "utils/uj_crc.c"

static void test_crc(void **state)
{
	UNUSED_STATE(state);

	assert_true(uj_crc32("coverage.lhc") == 0xF5BE063A);
	assert_true(uj_crc32("123456789") == 0xCBF43926);
	assert_true(uj_crc32("crc-32-ieee 802.3") == 0x184FD626);
}

int main(void)
{
	const struct CMUnitTest tests[] = {cmocka_unit_test(test_crc)};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
