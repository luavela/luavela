/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common_lua.h"

static int __index(lua_State *L)
{
	lua_gc(L, LUA_GCCOLLECT, 0);
	lua_pushnil(L);
	return 1;
}

static void test_lua_getfield(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = luaL_newstate();

	lua_newtable(L);

	lua_pushstring(L, "myval");
	lua_setfield(L, -2, "key1");

	assert_stack_size(L, 1);

	lua_getfield(L, -1, "key1");

	assert_stack_size(L, 2);
	assert_true(lua_isstring(L, -1));
	lua_pop(L, 1);

	lua_getfield(L, -1, "key2");
	assert_true(lua_isnil(L, -1));
	lua_pop(L, 1);

	lua_newtable(L);

	lua_pushstring(L, "__index");
	lua_pushcfunction(L, __index);
	lua_settable(L, -3);

	lua_setmetatable(L, -2);

	lua_pushstring(L, "myval");
	lua_getfield(L, -2, "key3");

	assert_true(lua_isnil(L, -1));
	assert_stack_size(L, 3);

	lua_close(L);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_lua_getfield),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
