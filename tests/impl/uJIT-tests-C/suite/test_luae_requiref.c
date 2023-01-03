/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common_lua.h"

static int luaopen_test_module1(lua_State *L)
{
	lua_newtable(L);
	lua_pushinteger(L, 42);
	lua_setfield(L, -2, "MAGIC_CONSTANT");
	return 1;
}

static int return_string_func(lua_State *L)
{
	lua_pushstring(L, "hello");
	return 1;
}

static int luaopen_test_module2(lua_State *L)
{
	lua_newtable(L);
	lua_pushcfunction(L, return_string_func);
	lua_setfield(L, -2, "return_string_func");
	return 1;
}

static void test_luae_requiref(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();

	/* module1 populates '_LOADED' */
	luaE_requiref(L, "test.module1", luaopen_test_module1);
	assert_stack_size(L, 1);

	lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
	lua_getfield(L, -1, "test.module1");

	assert_stack_size(L, 3);
	assert_true(lua_equal(L, -1, -3));

	lua_pop(L, 2);
	assert_stack_size(L, 1);

	/* module2 populates '_LOADED' */
	luaE_requiref(L, "test.module2", luaopen_test_module2);

	lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
	lua_getfield(L, -1, "test.module2");

	assert_stack_size(L, 4);
	assert_true(lua_equal(L, -1, -3));

	lua_pop(L, 2);
	assert_stack_size(L, 2);

	/* module1 isn't equal to module2 */
	assert_false(lua_equal(L, -1, -2));

	lua_close(L);
}

static void test_require(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();

	const char *module1_not_loaded =
		"assert(package.loaded['test.module1'] == nil)";

	const char *module2_not_loaded =
		"assert(package.loaded['test.module2'] == nil)";

	const char *module1_loaded =
		"assert(_G['test.module1'] == nil)\n"
		"assert(type(package.loaded['test.module1']) == 'table')\n"
		"local module1 = require 'test.module1'\n"
		"assert(module1.MAGIC_CONSTANT == 42)";

	const char *module2_loaded =
		"assert(_G['test.module2'] == nil)\n"
		"assert(type(package.loaded['test.module2']) == 'table')\n"
		"local module2 = require 'test.module2'\n"
		"assert(module2.return_string_func() == 'hello')";

	luaL_openlibs(L);

	assert_true(luaL_dostring(L, module1_not_loaded) == 0);
	assert_true(luaL_dostring(L, module2_not_loaded) == 0);

	luaE_requiref(L, "test.module1", luaopen_test_module1);
	luaE_requiref(L, "test.module2", luaopen_test_module2);

	assert_stack_size(L, 2);
	lua_pop(L, 2);

	assert_true(luaL_dostring(L, module1_loaded) == 0);
	assert_true(luaL_dostring(L, module2_loaded) == 0);

	lua_close(L);
}

int main(void)
{
	const struct CMUnitTest tests[] = {cmocka_unit_test(test_luae_requiref),
					   cmocka_unit_test(test_require)};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
