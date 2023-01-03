/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common.h"
#include <string.h>
#include <ctype.h>

#include <utils/random.c>

#define TEST_BUFFER_SIZE 8

static void test_random_hex_file_extension(void **state)
{
	UNUSED_STATE(state);

	char buffer[TEST_BUFFER_SIZE];

	memset(buffer, 0, sizeof(char) * TEST_BUFFER_SIZE);
	random_hex_file_extension(buffer, 0);
	assert_true(strlen(buffer) == 0);

	memset(buffer, 0, sizeof(char) * TEST_BUFFER_SIZE);
	random_hex_file_extension(buffer, 1);
	assert_true(buffer[0] == '.' && strlen(buffer) == 1);

	memset(buffer, 0, sizeof(char) * TEST_BUFFER_SIZE);
	random_hex_file_extension(buffer, 2);
	assert_true(buffer[0] == '.' && isxdigit((int)buffer[1]) &&
		    strlen(buffer) == 2);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_random_hex_file_extension),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
