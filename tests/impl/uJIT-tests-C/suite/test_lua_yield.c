/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * This test is based on the reproducer provided by Arseny Vakhrushev
 * within https://github.com/luavela/luavela/issues/12.
 */

#include "test_common_lua.h"

/* To test passing values via stack on yield: */
#define YIELD_STATUS "yielded"

static int yield(lua_State *L)
{
	assert_stack_size(L, 0);
	lua_pushliteral(L, YIELD_STATUS);
	return lua_yield(L, lua_gettop(L));
}

static void assert_yielded(lua_State *L, const char *status)
{
	assert_int_equal(lua_status(L), LUA_YIELD);
	assert_stack_size(L, 1);
	test_string(L, -1, status);
	lua_pop(L, 1);
}

static void test_lua_yield(void **state)
{
	UNUSED_STATE(state);
	lua_State *L = luaL_newstate();
	lua_State *T = lua_newthread(L);
	const char *coro1 = "canary = 'ENTER #1'; yield(); canary = 'LEAVE #1'";
	const char *coro2 = "canary = 'ENTER #2'; yield(); canary = 'LEAVE #2'";

	luaL_openlibs(L);
	lua_pushcfunction(L, yield);
	lua_setglobal(L, "yield");

	assert_stack_size(L, 1);
	assert_stack_size(T, 0);

	assert_int_equal(luaL_loadstring(T, coro1), 0);

	lua_resume(T, 0); /* ENTER #1 */
	assert_yielded(T, YIELD_STATUS);
	test_global_string(T, "canary", "ENTER #1");

	lua_resume(T, 0); /* LEAVE #1 */
	assert_stack_size(T, 0);
	test_global_string(T, "canary", "LEAVE #1");

	/*
	 * Normally finished thread can be reused:
	 * https://www.lua.org/manual/5.3/manual.html#lua_status
	 */
	assert_int_equal(luaL_loadstring(T, coro2), 0);

	lua_resume(T, 0); /* ENTER #2 */
	assert_yielded(T, YIELD_STATUS);
	test_global_string(T, "canary", "ENTER #2");

	lua_resume(T, 0); /* LEAVE #2 */
	assert_stack_size(T, 0);
	test_global_string(T, "canary", "LEAVE #2");

	lua_close(L);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_lua_yield),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
