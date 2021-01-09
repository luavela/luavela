/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common_lua.h"

static int __lt(lua_State *L)
{
	lua_pushboolean(L, 1);
	lua_gc(L, LUA_GCCOLLECT, 0);
	return 1;
}

static void test_lua_lessthan(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();

	lua_pushnumber(L, 1);
	lua_pushnumber(L, 1);

	assert_false(lua_lessthan(L, -1, -2));
	lua_pop(L, 2);

	lua_newtable(L);
	lua_newtable(L);
	lua_newtable(L);

	assert_stack_size(L, 3);

	/* Setup __lt metamethod */
	lua_pushstring(L, "__lt");
	lua_pushcfunction(L, __lt);
	lua_settable(L, -3);

	lua_pushvalue(L, -1);
	assert_stack_size(L, 4);

	lua_setmetatable(L, -3);
	lua_setmetatable(L, -3);

	assert_true(lua_lessthan(L, -1, -2));

	lua_pushnumber(L, 150);
	assert_true(lua_lessthan(L, -1, -3));
	assert_stack_size(L, 3);

	assert_true(lua_lessthan(L, -1, -2));
	assert_true(lua_lessthan(L, -1, -2));

	lua_close(L);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_lua_lessthan),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
