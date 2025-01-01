/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common_lua.h"
#include "lj_obj.h"

static void assert_watermark(lua_State *L)
{
	/*
	 * Here we try to check ITERN 'hidden' control variable correctness
	 * by testing stack content above L->top which is dubious
	 * but no other method come to mind.
	 */
	const TValue *idx = L->top + 5;
	assert_true(idx->u32.hi == LJ_ITERN_MARK);
}

static void test_jitpairs(void **state)
{
	UNUSED_STATE(state);

	const char *chunk =
		"jit.opt.start(4, 'hotloop=1', 'hotexit=1')\n"
		"local t = {1, 2, 3, 4, 5, a=6, b=7, c=8, d=9, e=10}\n"
		"for _, _ in pairs(t) do\n"
		"end";
	lua_State *L = test_lua_open();

	luaL_openlibs(L);
	assert_true(luaL_loadstring(L, chunk) == 0);
	lua_call(L, 0, 0);
	assert_watermark(L);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_jitpairs),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
