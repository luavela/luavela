/*
 * Tests for various cases of resizing coroutine's stack.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common_lua.h"
#include "lj_obj.h"

static size_t stack_size_internal(const struct lua_State *L)
{
	return L->stacksize;
}

static void test_stack_resize_rec_funcc(void **state)
{
	UNUSED_STATE(state);

	const char *chunk_fname = "chunks/test_stack_resize/rec_ff.lua";
	lua_State *L = test_lua_open();
	size_t stacksize1;
	size_t stacksize2;
	size_t stacksize3;

	luaL_openlibs(L);
	assert_int_equal(luaL_dofile(L, chunk_fname), 0);

	stacksize1 = stack_size_internal(L);

	lua_getglobal(L, "payload");
	lua_call(L, 0, 0);
	stacksize2 = stack_size_internal(L);

	assert_true(stacksize2 == stacksize1); /* stack was not resized */

	lua_getglobal(L, "payload");
	lua_call(L, 0, 0);
	stacksize3 = stack_size_internal(L);

	assert_true(stacksize3 > stacksize2); /* stack was resized */

	lua_close(L);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_stack_resize_rec_funcc)};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
