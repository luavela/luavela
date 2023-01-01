/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common_lua.h"

static int lib_func1(lua_State *L)
{
	test_string(L, lua_upvalueindex(1), "UPVALUE 1");
	lua_pushboolean(L, 1);
	return 1;
}

static int lib_func2(lua_State *L)
{
	test_string(L, lua_upvalueindex(2), "UPVALUE 2");
	lua_pushboolean(L, 1);
	return 1;
}

struct luaL_Reg lib[] = {
	{"func1", lib_func1},
	{"func2", lib_func2},
	{NULL, NULL} /* sentinel */
};

static void test_openlib(void **state)
{
	UNUSED_STATE(state);
	const char *chunk = "local lib = require('api.lib') \n"
			    "assert(type(lib) == 'table')   \n"
			    "assert(lib.func1())            \n"
			    "assert(lib.func2())            \n";
	lua_State *L = test_lua_open();

	luaL_openlibs(L);

	lua_pushstring(L, "UPVALUE 1");
	lua_pushstring(L, "UPVALUE 2");
	luaL_openlib(L, "api.lib", lib, 2);
	assert_stack_size(L, 1);

	/*
	 * Removing the module table and enforcing a full GC cycle should be
	 * harmless: all upvalues are anchored to C closures, the module
	 * table is anchored through REGISTRY._LOADED table.
	 */
	lua_pop(L, 1);
	lua_gc(L, LUA_GCCOLLECT, 0);
	assert_stack_size(L, 0);

	assert_int_equal(luaL_dostring(L, chunk), 0);

	lua_close(L);
}

int main(void)
{
	const struct CMUnitTest tests[] = {cmocka_unit_test(test_openlib)};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
