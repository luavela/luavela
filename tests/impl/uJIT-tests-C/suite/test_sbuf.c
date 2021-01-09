/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common_lua.h"
#include "uj_sbuf.h"
#include "uj_dispatch.h"

static int overflow(struct lua_State *L)
{
	/* Request a really big buffer */
	uj_sbuf_reserve(uj_sbuf_reset_tmp(L), UINT64_MAX);
	return 0;
}

/* Tests string buffer overflow */
static void test_sbuf_overflow(void **state)
{
	UNUSED_STATE(state);

	struct lua_State *L = test_lua_open();
	int status = lua_cpcall(L, overflow, NULL);

	assert_int_not_equal(status, 0);
	/* Error code is specific */
	assert_int_not_equal(status, LUA_ERRMEM);
	/* Error message says something about string buffer */
	test_substring(L, -1, "string buffer");
	lua_close(L);
}

/* Tests push into zero-sized buffer */
static void test_zero_sbuf(void **state)
{
	UNUSED_STATE(state);

	struct sbuf sb;
	const char *str = "Hello, world!";
	struct lua_State *L = test_lua_open();

	uj_sbuf_init(L, &sb);
	/* May last forever if buf growing is incorrect: */
	uj_sbuf_push_cstr(&sb, str);
	assert_int_equal(uj_sbuf_size(&sb), strlen(str));
	assert_string_equal(uj_sbuf_front(&sb), str);
	uj_sbuf_free(L, &sb);
	lua_close(L);
}

int main(void)
{
	const struct CMUnitTest tests[] = {cmocka_unit_test(test_sbuf_overflow),
					   cmocka_unit_test(test_zero_sbuf)};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
