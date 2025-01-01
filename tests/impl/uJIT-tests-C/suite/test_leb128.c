/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common.h"

#include <stdint.h>
#include <string.h>

#include <utils/leb128.c>

#define BUFFER_SIZE 16

static void test_write_uleb128(void **state)
{
	UNUSED_STATE(state);

	size_t bytes_written = 0;
	uint8_t buffer[BUFFER_SIZE] = {0};

	bytes_written = write_uleb128(buffer, 0);
	assert_true(bytes_written == 1);
	assert_true(buffer[0] == 0x0);

	bytes_written = write_uleb128(buffer, 64);
	assert_true(bytes_written == 1);
	assert_true(buffer[0] == 0x40);

	bytes_written = write_uleb128(buffer, 128);
	assert_true(bytes_written == 2);
	assert_true(buffer[0] == 0x80 && buffer[1] == 0x01);

	bytes_written = write_uleb128(buffer, UINT64_MAX);
	assert_true(bytes_written == 10);
	assert_true(buffer[0] == 0xff && buffer[1] == 0xff &&
		    buffer[2] == 0xff && buffer[3] == 0xff &&
		    buffer[4] == 0xff && buffer[5] == 0xff &&
		    buffer[6] == 0xff && buffer[7] == 0xff &&
		    buffer[8] == 0xff && buffer[9] == 0x01);
}

static void test_write_leb128(void **state)
{
	UNUSED_STATE(state);

	size_t bytes_written = 0;
	uint8_t buffer[BUFFER_SIZE] = {0};

	bytes_written = write_leb128(buffer, 0);
	assert_true(bytes_written == 1);
	assert_true(buffer[0] == 0x0);

	bytes_written = write_leb128(buffer, -624485);
	assert_true(bytes_written == 3);
	assert_true(buffer[0] == 0x9b && buffer[1] == 0xf1 &&
		    buffer[2] == 0x59);

	bytes_written = write_leb128(buffer, INT64_MIN);
	assert_true(bytes_written == 10);
	assert_true(buffer[0] == 0x80 && buffer[1] == 0x80 &&
		    buffer[2] == 0x80 && buffer[3] == 0x80 &&
		    buffer[4] == 0x80 && buffer[5] == 0x80 &&
		    buffer[6] == 0x80 && buffer[7] == 0x80 &&
		    buffer[8] == 0x80 && buffer[9] == 0x7f);

	bytes_written = write_leb128(buffer, INT64_MAX);
	assert_true(bytes_written == 10);
	assert_true(buffer[0] == 0xff && buffer[1] == 0xff &&
		    buffer[2] == 0xff && buffer[3] == 0xff &&
		    buffer[4] == 0xff && buffer[5] == 0xff &&
		    buffer[6] == 0xff && buffer[7] == 0xff &&
		    buffer[8] == 0xff && buffer[9] == 0x00);
}

/* Test miscellanious writes, both signed and unsigned */
static void test_misc_writes(void **state)
{
	UNUSED_STATE(state);

	size_t bytes_written = 0;
	uint8_t buffer[BUFFER_SIZE] = {0};

	bytes_written = write_leb128(buffer, 0x10);
	assert_true(bytes_written == 1);
	assert_true(buffer[0] == 0x10);

	bytes_written = write_uleb128(buffer, 0x10);
	assert_true(bytes_written == 1);
	assert_true(buffer[0] == 0x10);

	bytes_written = write_leb128(buffer, -0x3b);
	assert_true(bytes_written == 1);
	assert_true(buffer[0] == 0x45);

	bytes_written = write_uleb128(buffer, 0x45);
	assert_true(bytes_written == 1);
	assert_true(buffer[0] == 0x45);

	bytes_written = write_leb128(buffer, 0x190e);
	assert_true(bytes_written == 2);
	assert_true(buffer[0] == 0x8e && buffer[1] == 0x32);

	bytes_written = write_uleb128(buffer, 0x190e);
	assert_true(bytes_written == 2);
	assert_true(buffer[0] == 0x8e && buffer[1] == 0x32);

	bytes_written = write_leb128(buffer, -0x143f);
	assert_true(bytes_written == 2);
	assert_true(buffer[0] == 0xc1 && buffer[1] == 0x57);

	bytes_written = write_uleb128(buffer, 0x2bc1);
	assert_true(bytes_written == 2);
	assert_true(buffer[0] == 0xc1 && buffer[1] == 0x57);

	bytes_written = write_leb128(buffer, 0x7e00000);
	assert_true(bytes_written == 4);
	assert_true(buffer[0] == 0x80 && buffer[1] == 0x80 &&
		    buffer[2] == 0x80 && buffer[3] == 0x3f);

	bytes_written = write_uleb128(buffer, 0x7e00000);
	assert_true(bytes_written == 4);
	assert_true(buffer[0] == 0x80 && buffer[1] == 0x80 &&
		    buffer[2] == 0x80 && buffer[3] == 0x3f);

	bytes_written = write_leb128(buffer, -0x6200000);
	assert_true(bytes_written == 4);
	assert_true(buffer[0] == 0x80 && buffer[1] == 0x80 &&
		    buffer[2] == 0x80 && buffer[3] == 0x4f);

	bytes_written = write_uleb128(buffer, 0x9e00000);
	assert_true(bytes_written == 4);
	assert_true(buffer[0] == 0x80 && buffer[1] == 0x80 &&
		    buffer[2] == 0x80 && buffer[3] == 0x4f);
}

static void test_read_uleb128(void **state)
{
	UNUSED_STATE(state);

	uint64_t value = 0;
	size_t bytes_read = 0;
	uint8_t buffer[BUFFER_SIZE] = {0};

	bytes_read = read_uleb128(&value, buffer);
	assert_true(bytes_read == 1);
	assert_true(value == 0);

	buffer[0] = 0x40;
	bytes_read = read_uleb128(&value, buffer);
	assert_true(bytes_read == 1);
	assert_true(value == 64);

	buffer[0] = 0x80;
	buffer[1] = 0x01;
	bytes_read = read_uleb128(&value, buffer);
	assert_true(bytes_read == 2);
	assert_true(value == 128);

	memset(buffer, 0xff, 9);
	buffer[9] = 0x01;
	bytes_read = read_uleb128(&value, buffer);
	assert_true(bytes_read == 10);
	assert_true(value == UINT64_MAX);
}

static void test_read_leb128(void **state)
{
	UNUSED_STATE(state);

	int64_t value = 0;
	size_t bytes_read = 0;
	uint8_t buffer[BUFFER_SIZE] = {0};

	bytes_read = read_leb128(&value, buffer);
	assert_true(bytes_read == 1);
	assert_true(value == 0);

	buffer[0] = 0x9b;
	buffer[1] = 0xf1;
	buffer[2] = 0x59;
	bytes_read = read_leb128(&value, buffer);
	assert_true(bytes_read == 3);
	assert_true(value == (int64_t)-624485);

	memset(buffer, 0x80, 9);
	buffer[9] = 0x7f;
	bytes_read = read_leb128(&value, buffer);
	assert_true(bytes_read == 10);
	assert_true(value == INT64_MIN);

	memset(buffer, 0xff, 9);
	buffer[9] = 0x00;
	bytes_read = read_leb128(&value, buffer);
	assert_true(bytes_read == 10);
	assert_true(value == INT64_MAX);
}

/* Test miscellanious reads, both signed and unsigned */
static void test_misc_reads(void **state)
{
	UNUSED_STATE(state);

	int64_t i_value = 0;
	uint64_t u_value = 0;

	size_t bytes_read = 0;
	uint8_t buffer[BUFFER_SIZE] = {0};

	buffer[0] = 0x10;

	bytes_read = read_leb128(&i_value, buffer);
	assert_true(bytes_read == 1);
	assert_true(i_value == (int64_t)0x10);

	bytes_read = read_uleb128(&u_value, buffer);
	assert_true(bytes_read == 1);
	assert_true(u_value == (uint64_t)0x10);

	buffer[0] = 0x45;

	bytes_read = read_leb128(&i_value, buffer);
	assert_true(bytes_read == 1);
	assert_true(i_value == (int64_t)-0x3b);

	bytes_read = read_uleb128(&u_value, buffer);
	assert_true(bytes_read == 1);
	assert_true(u_value == (uint64_t)0x45);

	buffer[0] = 0x8e;
	buffer[1] = 0x32;

	bytes_read = read_leb128(&i_value, buffer);
	assert_true(bytes_read == 2);
	assert_true(i_value == (int64_t)0x190e);

	bytes_read = read_uleb128(&u_value, buffer);
	assert_true(bytes_read == 2);
	assert_true(u_value == (uint64_t)0x190e);

	buffer[0] = 0xc1;
	buffer[1] = 0x57;

	bytes_read = read_leb128(&i_value, buffer);
	assert_true(bytes_read == 2);
	assert_true(i_value == (int64_t)-0x143f);

	bytes_read = read_uleb128(&u_value, buffer);
	assert_true(bytes_read == 2);
	assert_true(u_value == (uint64_t)0x2bc1);

	buffer[0] = 0xc1;
	buffer[1] = 0x57;

	bytes_read = read_leb128(&i_value, buffer);
	assert_true(bytes_read == 2);
	assert_true(i_value == (int64_t)-0x143f);

	bytes_read = read_uleb128(&u_value, buffer);
	assert_true(bytes_read == 2);
	assert_true(u_value == (uint64_t)0x2bc1);

	buffer[0] = 0x80;
	buffer[1] = 0x80;
	buffer[2] = 0x80;
	buffer[3] = 0x3f;

	bytes_read = read_leb128(&i_value, buffer);
	assert_true(bytes_read == 4);
	assert_true(i_value == (int64_t)0x7e00000);

	bytes_read = read_uleb128(&u_value, buffer);
	assert_true(bytes_read == 4);
	assert_true(u_value == (uint64_t)0x7e00000);

	buffer[0] = 0x80;
	buffer[1] = 0x80;
	buffer[2] = 0x80;
	buffer[3] = 0x4f;

	bytes_read = read_leb128(&i_value, buffer);
	assert_true(bytes_read == 4);
	assert_true(i_value == (int64_t)-0x6200000);

	bytes_read = read_uleb128(&u_value, buffer);
	assert_true(bytes_read == 4);
	assert_true(u_value == (uint64_t)0x9e00000);
}

static void test_guarded_reads(void **state)
{
	UNUSED_STATE(state);

	int64_t i_value = 0;
	uint64_t u_value = 0;

	size_t bytes_read = 0;
	uint8_t buffer[BUFFER_SIZE] = {0};

	/* Reading cannot consume less than 1 byte, so this will always fail: */
	assert_true(read_leb128_n(&i_value, buffer, 0) == 0);
	assert_true(read_uleb128_n(&u_value, buffer, 0) == 0);

	bytes_read = read_leb128_n(&i_value, buffer, 1);
	assert_true(bytes_read == 1);
	assert_true(i_value == 0);

	bytes_read = read_uleb128_n(&u_value, buffer, 1);
	assert_true(bytes_read == 1);
	assert_true(u_value == 0);

	buffer[0] = 0x80;
	buffer[1] = 0x80;
	buffer[2] = 0x80;
	buffer[3] = 0x3f;

	assert_true(read_leb128_n(&i_value, buffer, 3) == 0);
	assert_true(read_uleb128_n(&u_value, buffer, 3) == 0);
	/* Values are untouched in case of failure: */
	assert_true(i_value == 0);
	assert_true(u_value == 0);

	bytes_read = read_leb128_n(&i_value, buffer, 4);
	assert_true(bytes_read == 4);
	assert_true(i_value == (int64_t)0x7e00000);

	bytes_read = read_uleb128_n(&u_value, buffer, 4);
	assert_true(bytes_read == 4);
	assert_true(u_value == (uint64_t)0x7e00000);

	bytes_read = read_leb128_n(&i_value, buffer, 5);
	assert_true(bytes_read == 4);
	assert_true(i_value == (int64_t)0x7e00000);

	bytes_read = read_uleb128_n(&u_value, buffer, 5);
	assert_true(bytes_read == 4);
	assert_true(u_value == (uint64_t)0x7e00000);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_write_uleb128),
		cmocka_unit_test(test_write_leb128),
		cmocka_unit_test(test_misc_writes),
		cmocka_unit_test(test_read_uleb128),
		cmocka_unit_test(test_read_leb128),
		cmocka_unit_test(test_misc_reads),
		cmocka_unit_test(test_guarded_reads),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
