/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common_lua.h"
#include "uj_sbuf.h"
#include "uj_dispatch.h"
#include "utils/uj_crc.h"

static void test_coverage_null_cb(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();
	/* Coverage with NULL callback will not start */
	int status = luaE_coveragestart_cb(L, NULL, NULL, NULL, 0);

	assert_int_equal(status, LUAE_COV_ERROR);
	/* But Lua state is still valid and can operate */
	luaL_openlibs(L);
	assert_int_equal(luaL_dostring(L, "print('Hello, world!')"), 0);
	lua_close(L);
}

static void dumper(void *data, const char *str, size_t num)
{
	struct sbuf *sb = data;

	uj_sbuf_push_block(sb, str, num);
}

static void test_coverage_cb(void **state)
{
	UNUSED_STATE(state);

	int status;
	lua_State *L = test_lua_open();
	struct sbuf *sb = uj_sbuf_reset_tmp(L);
	const char *chunk = "for i = 1, 3 do         \n"
			    "  print('Hello, world!')\n"
			    "end                     \n";

	status = luaE_coveragestart_cb(L, dumper, sb, NULL, 0);
	assert_int_equal(status, LUAE_COV_SUCCESS);

	luaL_openlibs(L);
	assert_int_equal(luaL_dostring(L, chunk), 0);

	/* Check string buffer content */
	assert_int_equal(uj_crc32(uj_sbuf_front(sb)), 1565529425);
	assert_int_equal(uj_sbuf_size(sb), 179);

	lua_close(L);
}

static void test_coverage_file(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();
	/* Coverage with NULL filename will not start */
	int status = luaE_coveragestart(L, NULL, NULL, 0);

	assert_int_equal(status, LUAE_COV_ERROR);
	/* But Lua state is still valid and can operate */
	luaL_openlibs(L);
	assert_int_equal(luaL_dostring(L, "print('Hello, world!')"), 0);
	lua_close(L);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_coverage_null_cb),
		cmocka_unit_test(test_coverage_cb),
		cmocka_unit_test(test_coverage_file)};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
