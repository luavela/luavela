/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common_lua.h"
#include "lj_def.h"

static int eq_metamethod(lua_State *L)
{
	lua_pushboolean(L, 1);
	lua_gc(L, LUA_GCCOLLECT, 0);
	return 1;
}

static void test_itself(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();

	lua_newtable(L);
	lua_newtable(L);

	assert_false(lua_equal(L, 1, 2));
	assert_stack_size(L, 2);
	assert_true(lua_equal(L, 1, 1));
	assert_true(lua_equal(L, 2, 2));

	lua_pushnumber(L, 15);
	lua_pushnumber(L, 22);
	lua_pushnumber(L, 15);

	assert_true(lua_equal(L, -1, -3));
	assert_false(lua_equal(L, -1, -2));
	assert_false(lua_equal(L, 1, -1));

	lua_pushboolean(L, 1);
	lua_pushboolean(L, 0);
	assert_false(lua_equal(L, -1, -2));

	lua_pushlightuserdata(L, (void *)L);
	UJ_PEDANTIC_OFF
	/* casting a function ptr to void* */
	lua_pushlightuserdata(L, (void *)test_itself);
	UJ_PEDANTIC_ON
	assert_false(lua_equal(L, -1, -2));
	assert_true(lua_equal(L, -1, -1));

	lua_close(L);
}

static void test_mm(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();

	lua_newtable(L);
	lua_newtable(L);
	lua_newtable(L);

	assert_false(lua_equal(L, 1, 2));
	assert_stack_size(L, 3);

	/* Overload __eq */
	lua_pushstring(L, "__eq");
	lua_pushcfunction(L, eq_metamethod);
	lua_settable(L, 3);

	lua_pushvalue(L, 3);
	assert_stack_size(L, 4);

	/* Set metatable to first table */
	lua_setmetatable(L, 1);
	lua_setmetatable(L, 2);

	assert_true(lua_equal(L, 1, 2));

	lua_pushnumber(L, 150);
	assert_false(lua_equal(L, 1, 3));
	assert_stack_size(L, 3);

	assert_true(lua_equal(L, 2, 1));
	assert_true(lua_equal(L, 1, 2));

	lua_close(L);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_itself),
		cmocka_unit_test(test_mm),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
