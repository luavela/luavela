/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common_lua.h"

static void assert_createstate(const struct luae_Options *opt)
{
	const char chunk[] = "local s = \"jjhkjhkhiuhjkbnbbjhhjhkksggg\"\n"
			     "local tconcat = table.concat              \n"
			     "local t = {}                              \n"
			     "t[s] = 323234	                           \n"
			     "tconcat(t, \",\")                         \n"
			     "local sum = 0                             \n"
			     "for _, v in next, t do sum = sum + v end \n";

	lua_State *L = luaE_createstate(opt);

	assert_non_null(L);
	assert_stack_size(L, 0);

	luaL_openlibs(L);

	assert_int_equal(luaL_dostring(L, chunk), 0);

	lua_close(L);
}

static void test_newstate_null_opt(void **state)
{
	UNUSED_STATE(state);

	assert_createstate(NULL);
}

static void test_newstate_default_opt(void **state)
{
	UNUSED_STATE(state);

	const struct luae_Options opt = {0};

	assert_createstate(&opt);
}

static void test_newstate_murmur(void **state)
{
	UNUSED_STATE(state);

	struct luae_Options opt = {0};

	opt.hashftype = LUAE_HASHF_MURMUR;
	assert_createstate(&opt);
}

static void test_newstate_disable_itern(void **state)
{
	UNUSED_STATE(state);

	struct luae_Options opt = {0};

	opt.disableitern = 1;
	assert_createstate(&opt);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_newstate_null_opt),
		cmocka_unit_test(test_newstate_default_opt),
		cmocka_unit_test(test_newstate_murmur),
		cmocka_unit_test(test_newstate_disable_itern)};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
