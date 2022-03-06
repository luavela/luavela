/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common.h"

#include <string.h>
#include <utils/lj_char.c>
#include <utils/str.c>

#define TEST_STR_BUFFER_SIZE 256

static void test_to_lower(void **state)
{
	UNUSED_STATE(state);

	char buffer[TEST_STR_BUFFER_SIZE];

	memset(buffer, 0, TEST_STR_BUFFER_SIZE);

	to_lower(
		buffer,
		"1234567890abcdefghijklmnopqrstuvwxyz!@#$%^&*()-_=+`~[]{}\\|/?>.<,");
	assert_string_equal(
		buffer,
		"1234567890abcdefghijklmnopqrstuvwxyz!@#$%^&*()-_=+`~[]{}\\|/?>.<,");

	to_lower(
		buffer,
		"1234567890AbCdEfGhIjKlMnOpQrStUvWxYz!@#$%^&*()-_=+`~[]{}\\|/?>.<,");
	assert_string_equal(
		buffer,
		"1234567890abcdefghijklmnopqrstuvwxyz!@#$%^&*()-_=+`~[]{}\\|/?>.<,");

	to_lower(buffer, "ABCDEFGHIJKlMNOPQRSTUVWXYZ");
	assert_string_equal(buffer, "abcdefghijklmnopqrstuvwxyz");
}

static void test_replace_underscores(void **state)
{
	UNUSED_STATE(state);

	char buffer[TEST_STR_BUFFER_SIZE] = {0};

	strncpy(buffer, "foo", TEST_STR_BUFFER_SIZE);
	replace_underscores(buffer);
	assert_string_equal(buffer, "foo");

	strncpy(buffer, "f_o_o", TEST_STR_BUFFER_SIZE);
	replace_underscores(buffer);
	assert_string_equal(buffer, "f.o.o");

	strncpy(buffer, "_f_o_o_", TEST_STR_BUFFER_SIZE);
	replace_underscores(buffer);
	assert_string_equal(buffer, ".f.o.o.");
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_to_lower),
		cmocka_unit_test(test_replace_underscores),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
