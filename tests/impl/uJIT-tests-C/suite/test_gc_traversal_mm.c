/*
 * Test cases:
 *  * Traversing coroutine stack inside GC during metamethod call from C API
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common_lua.h"

static int mm_cat(lua_State *L)
{
	lua_pushstring(L, "tab1");
	lua_pushstring(L, "tab2");
	test_enforce_gc_cycle(L);
	/* The following call will trigger GC */
	lua_concat(L, 2);
	return 1;
}

static void test_gc_traversal_mm_capi(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();

	/*
	 * local tab1 = {}
	 * setmetatable(tab1, {__concat = mm_cat})
	 * local tab2 = {}
	 */
	lua_createtable(L, 0, 16);
	lua_createtable(L, 0, 16);
	lua_pushcfunction(L, mm_cat);
	lua_setfield(L, -2, "__concat");
	lua_setmetatable(L, -2);
	lua_createtable(L, 0, 16);

	/* tab1 .. tab2 */
	assert_stack_size(L, 2);
	lua_concat(L, 2);

	assert_stack_size(L, 1);
	test_string(L, 1, "tab1tab2");

	lua_close(L);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_gc_traversal_mm_capi),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
