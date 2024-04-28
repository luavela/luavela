/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common_lua.h"

/* "Allocator" that emulates ERRMEM (out of memory error). */
static void *allocf_errmem(void *ud, void *ptr, size_t osize, size_t nsize)
{
	UNUSED(ud);
	UNUSED(ptr);
	UNUSED(osize);
	UNUSED(nsize);

	return NULL;
}

static void test_errmem_stack_sync(void **state)
{
	UNUSED_STATE(state);

	lua_State *L;
	lua_Alloc allocf_orig;
	void *state_orig;

	const char *chunk =
		"local function func5()        \n"
		"        local t = { x = 'y' } \n" /* new table allocation */
		"end                           \n"
		"local function func4()        \n"
		"        func5()               \n"
		"end                           \n"
		"local function func3()        \n"
		"        func4()               \n"
		"end                           \n"
		"local function func2()        \n"
		"        func3()               \n"
		"end                           \n"
		"function func1()              \n" /* global function */
		"        func2()               \n"
		"end                           \n";

	L = test_lua_open();

	assert_int_equal(luaL_dostring(L, chunk), 0);

	lua_getglobal(L, "func1");
	assert_stack_size(L, 1);

	allocf_orig = lua_getallocf(L, &state_orig);

	lua_setallocf(L, allocf_errmem, NULL);
	assert_int_equal(lua_pcall(L, 0, 0, 0), LUA_ERRMEM);
	lua_setallocf(L, allocf_orig, state_orig);

	assert_stack_size(L, 1);
	test_substring(L, 1, "not enough memory");

	lua_close(L);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_errmem_stack_sync),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
