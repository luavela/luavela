/*
 * This is a part of uJIT's extension library testing suite
 * Tests verify that C API functions of ujit.table module are callable
 * i.e. don't fail when called.
 * Call semantics of these calls are verified via their Lua counterparts
 * as the latter are just wrappers of C implementation supplied via
 * C API below.
 *
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common_lua.h"

/*
 * Type of an arbitrary function from ujit.table C API which creates a new
 * table of some type (specified in its name) from existing table located
 * on the stack at idx.
 */
typedef void (*create_table_from_idx_function)(lua_State *, int idx);

/*
 * Helper function to create the table on stack somewhere between other elements
 * on the stack.
 * Relative location of other elements on stack is arbitrary.
 * The only important thing is source (i.e. copied) table idx.
 *
 * This setting can be used for all the table API functions as the particular
 * stack layout is out of interest (see tests' semantics in the header).
 */
static int prepare_stack(lua_State *L)
{
	int idx_tab;

	/* Push something on stack to push table for copying in the middle. */
	lua_pushnumber(L, 4);
	lua_pushnumber(L, 93);

	/* Create a table at the top of the stack. */
	lua_newtable(L);
	idx_tab = lua_gettop(L);

	lua_pushnumber(L, 3989);

	return idx_tab;
}

static void assert_stack(lua_State *L, int stack_size, int idx)
{
	/* Test that object in the topmost slot is a table. */
	assert_true(lua_istable(L, -1));

	assert_stack_size(L, stack_size);

	/*
	 * Test that top object and idx-th object from the bottom are different.
	 */
	assert_false(lua_rawequal(L, -1, -(stack_size - idx)));
}

static void body(create_table_from_idx_function create_table)
{
	int stack_size;
	lua_State *L = test_lua_open();
	int idx = prepare_stack(L);

	stack_size = lua_gettop(L);
	create_table(L, idx);

	/* 1 element added - created table. */
	assert_stack(L, stack_size + 1, idx);

	lua_close(L);
}

static void test_table_shallowcopytable(void **state)
{
	UNUSED_STATE(state);

	body(luaE_shallowcopytable);
}

static void test_table_tablekeys(void **state)
{
	UNUSED_STATE(state);

	body(luaE_tablekeys);
}

static void test_table_tablevalues(void **state)
{
	UNUSED_STATE(state);

	body(luaE_tablevalues);
}

static void test_table_tabletoset(void **state)
{
	UNUSED_STATE(state);

	body(luaE_tabletoset);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_table_shallowcopytable),
		cmocka_unit_test(test_table_tablekeys),
		cmocka_unit_test(test_table_tablevalues),
		cmocka_unit_test(test_table_tabletoset)};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
