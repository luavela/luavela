/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "lj_obj.h"

#include "test_common_lua.h"

const char *foo = "function foo() return 1 end";
const char *bar = "function bar() return _G end";

static int cfunc(lua_State *L)
{
	UNUSED(L);

	return 0;
}

static int test_api_usesfenv(lua_State *L, int idx)
{
	int usesfenv;

	assert_stack_size(L, 1);
	usesfenv = luaE_usesfenv(L, idx);
	lua_pop(L, 1);
	assert_stack_size(L, 0);
	return usesfenv;
}

static void test_push_large_integer(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();
	const lua_Integer large_int = 0xdeaddeadbeefll;

	lua_pushinteger(L, large_int);
	assert_stack_size(L, 1);
	assert_true(lua_isnumber(L, -1));
	assert_int_equal(lua_tointeger(L, -1), large_int);

	lua_close(L);
}

static void test_api_args(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();
	int i = 0;

	luaL_openlibs(L);

	assert_int_equal(luaL_dostring(L, foo), 0);
	assert_int_equal(luaL_dostring(L, bar), 0);

	assert_stack_size(L, 0);

	lua_getfield(L, LUA_GLOBALSINDEX, "foo");
	assert_int_equal(test_api_usesfenv(L, 1), 0);

	lua_getfield(L, LUA_GLOBALSINDEX, "bar");
	assert_int_equal(test_api_usesfenv(L, 1), 1);

	lua_getfield(L, LUA_GLOBALSINDEX, "getmetatable");
	assert_int_equal(test_api_usesfenv(L, 1), 0);

	lua_pushcfunction(L, cfunc);
	assert_int_not_equal(test_api_usesfenv(L, 1), 0);

	lua_getfield(L, LUA_GLOBALSINDEX, "foo");
	assert_int_equal(test_api_usesfenv(L, -1), 0);

	lua_getfield(L, LUA_GLOBALSINDEX, "bar");
	assert_int_equal(test_api_usesfenv(L, -1), 1);

	lua_getfield(L, LUA_GLOBALSINDEX, "getmetatable");
	assert_int_equal(test_api_usesfenv(L, -1), 0);

	lua_pushcfunction(L, cfunc);
	assert_int_not_equal(test_api_usesfenv(L, -1), 0);

	lua_newtable(L);
	luaE_shallowcopytable(L, LUA_GLOBALSINDEX);
	luaE_shallowcopytable(L, -1);
	assert_stack_size(L, 3);

	/*
	 * All other API's will also call capi_ext_newtable(), but let's
	 * test them too.
	 */
	luaE_tablekeys(L, 1);
	luaE_tablekeys(L, -1);
	assert_stack_size(L, 5);

	luaE_tablevalues(L, 1);
	luaE_tablevalues(L, -1);
	assert_stack_size(L, 7);

	luaE_tabletoset(L, 1);
	luaE_tabletoset(L, -1);
	assert_stack_size(L, 9);

	for (; i < 9; i++)
		assert_int_equal(tvistab(L->stack + 1 + i), 1);

	lua_close(L);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_api_args),
		cmocka_unit_test(test_push_large_integer),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
