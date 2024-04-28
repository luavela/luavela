/*
 * Tests for the standard C-level debug API.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common_lua.h"

/* Testing lua_getstack */

static const char *chunk_getstack =
	" local function foo(x)         "
	"     assert(x, 'x is here')    "
	"     aux_getstack(x)           " /* defined in C  */
	" end                           "
	" foo('XX')                     ";

static int aux_getstack(lua_State *L)
{
	lua_Debug _ar0 = {0};
	const lua_Debug *ar0 = &_ar0;
	lua_Debug _ar = {0};
	lua_Debug *ar = &_ar;

	/* More complex things should be tested with lua_getinfo. */

	assert_int_equal(lua_getstack(L, 0, ar), 1); /* aux_getstack */
	assert_memory_not_equal(ar, ar0, sizeof(*ar));

	assert_int_equal(lua_getstack(L, 1, ar), 1); /* foo */
	assert_memory_not_equal(ar, ar0, sizeof(*ar));

	assert_int_equal(lua_getstack(L, 2, ar), 1); /* main chunk */
	assert_memory_not_equal(ar, ar0, sizeof(*ar));

	assert_int_equal(lua_getstack(L, 3, ar), 0); /* nothing here */

	assert_int_equal(lua_getstack(L, 4, ar), 0); /* nothing here */

	/*
	 * NB! The RefMan says nothing about how we should handle
	 * negative values for level.
	 */

	return 0;
}

static void test_lua_getstack(void **state)
{
	UNUSED(state);
	lua_State *L = test_lua_open();

	luaL_openlibs(L);
	lua_register(L, "aux_getstack", aux_getstack);
	assert_int_equal(luaL_dostring(L, chunk_getstack), 0);
	lua_close(L);
}

/* Testing lua_getlocal */

static const char *chunk_getlocal =
	" local function foo(x, y, ...) "
	"     local var1 = 'VAR1'       "
	"     assert(x, 'x is here')    "
	"     assert(y, 'y is here')    "
	"     aux_getlocal(x, y)        " /* defined in C  */
	" end                           "
	" local function bar()          "
	"     local var2 = 'VAR2'       "
	"     for i = 1, 1 do           "
	"         foo('XX', 'YY', 'ZZ') "
	"     end                       "
	" end                           "
	" bar()                         ";

static void assert_local_string(lua_State *L, lua_Debug *ar, int n,
				const char *exp_name, const char *exp_value)
{
	const char *name = lua_getlocal(L, ar, n);

	assert_string_equal(name, exp_name);
	test_string(L, -1, exp_value);
	lua_pop(L, 1);
}

static void assert_getlocal_c(lua_State *L, int level)
{
	lua_Debug _ar = {0};
	lua_Debug *ar = &_ar;
	const int stack_size = lua_gettop(L);

	assert_int_equal(lua_getstack(L, level, ar), 1);

	/*
	 * Function arguments. But we are in a C function, so there is no
	 * way to deduce argument names:
	 */

	assert_local_string(L, ar, 1, "(*temporary)", "XX");
	assert_local_string(L, ar, 2, "(*temporary)", "YY");

	/* Non-existent stack slot: */
	assert_null(lua_getlocal(L, ar, stack_size + 1));
	assert_int_equal(stack_size, lua_gettop(L)); /* nothing was pushed */

	/* C functions know nothing about variadic arguments: */
	assert_null(lua_getlocal(L, ar, -1));
	assert_int_equal(stack_size, lua_gettop(L)); /* nothing was pushed */
}

static void assert_getlocal_lua_vararg(lua_State *L, int level)
{
	lua_Debug _ar = {0};
	lua_Debug *ar = &_ar;

	assert_int_equal(lua_getstack(L, level, ar), 1);

	/* function's fixed arguments: */
	assert_local_string(L, ar, 1, "x", "XX");
	assert_local_string(L, ar, 2, "y", "YY");

	/* function's local variables: */
	assert_local_string(L, ar, 3, "var1", "VAR1");

	/* function's variadic arguments: */
	assert_local_string(L, ar, -1, "(*vararg)", "ZZ");

	/* Non-existent stack slots: */
	assert_null(lua_getlocal(L, ar, 10));
	assert_null(lua_getlocal(L, ar, -10));
}

static void assert_getlocal_lua_fixarg(lua_State *L, int level)
{
	lua_Debug _ar = {0};
	lua_Debug *ar = &_ar;
	const int for_idx_slot = 2;
	const char *name;

	assert_int_equal(lua_getstack(L, level, ar), 1);

	/* function's fixed arguments: */
	assert_local_string(L, ar, 1, "var2", "VAR2");

	/* numeric loop variables: */
	name = lua_getlocal(L, ar, for_idx_slot);
	assert_string_equal(name, "(for index)");
	name = lua_getlocal(L, ar, for_idx_slot + 1);
	assert_string_equal(name, "(for limit)");
	name = lua_getlocal(L, ar, for_idx_slot + 2);
	assert_string_equal(name, "(for step)");
	lua_pop(L, 3);

	/* Non-existent stack slots: */
	assert_true(lua_gettop(L) < 100);
	assert_null(lua_getlocal(L, ar, 100)); /* position out of frame */
	assert_null(lua_getlocal(L, ar, -100)); /* no variadic args at all */
}

static int aux_getlocal(lua_State *L)
{
	assert_stack_size(L, 2);
	assert_getlocal_c(L, 0);
	assert_getlocal_lua_vararg(L, 1);
	assert_getlocal_lua_fixarg(L, 2);
	assert_stack_size(L, 2);
	return 0;
}

static void test_lua_getlocal(void **state)
{
	UNUSED(state);
	lua_State *L = test_lua_open();

	luaL_openlibs(L);
	lua_register(L, "aux_getlocal", aux_getlocal);
	assert_int_equal(luaL_dostring(L, chunk_getlocal), 0);
	lua_close(L);
}

/* Testing lua_setlocal */

static const char *chunk_setlocal =
	" local function foo(a, x, y, b) "
	"     assert(a == 'AA')          "
	"     assert(x == 'XX')          "
	"     assert(y == 'YY')          "
	"     assert(b == 'BB')          "
	"     aux_setlocal()             " /* defined in C  */
	"     assert(a == 'AA')          "
	"     assert(x == 'YY')          "
	"     assert(y == 'XX')          "
	"     assert(b == 'BB')          "
	" end                            "
	" foo('AA', 'XX', 'YY', 'BB')    ";

static int aux_setlocal(lua_State *L)
{
	lua_Debug _ar = {0};
	lua_Debug *ar = &_ar;

	assert_int_equal(lua_getstack(L, 1, ar), 1);

	/* Function's fixed arguments: */
	assert_local_string(L, ar, 1, "a", "AA");
	assert_local_string(L, ar, 2, "x", "XX");
	assert_local_string(L, ar, 3, "y", "YY");
	assert_local_string(L, ar, 4, "b", "BB");

	/* Swap some argument values: */
	lua_pushliteral(L, "YY");
	assert_string_equal(lua_setlocal(L, ar, 2), "x");
	lua_pushliteral(L, "XX");
	assert_string_equal(lua_setlocal(L, ar, 3), "y");

	/* Assert that swap went well: */
	assert_local_string(L, ar, 1, "a", "AA");
	assert_local_string(L, ar, 2, "x", "YY");
	assert_local_string(L, ar, 3, "y", "XX");
	assert_local_string(L, ar, 4, "b", "BB");

	return 0;
}

static void test_lua_setlocal(void **state)
{
	UNUSED(state);
	lua_State *L = test_lua_open();

	luaL_openlibs(L);
	lua_register(L, "aux_setlocal", aux_setlocal);
	assert_int_equal(luaL_dostring(L, chunk_setlocal), 0);
	lua_close(L);
}

/* Testing lua_getinfo */

static const char *chunk_getinfo_short_src[] = {
	"-- Hello Lua!\naux_getinfo_short_src(0)", "aux_getinfo_short_src(1)"};

static const char *chunk_getinfo_exp_short_src[] = {
	"[string \"-- Hello Lua!...\"]",
	"[string \"aux_getinfo_short_src(1)\"]"};

static int aux_getinfo_short_src(lua_State *L)
{
	lua_Debug _ar = {0};
	lua_Debug *ar = &_ar;
	const int idx = (const int)lua_tointeger(L, -1);

	assert_int_equal(lua_getstack(L, 1, ar), 1);
	assert_int_not_equal(lua_getinfo(L, "S", ar), 0);
	assert_string_equal(ar->short_src, chunk_getinfo_exp_short_src[idx]);

	return 0;
}

static void test_lua_getinfo_short_src(void **state)
{
	UNUSED(state);
	lua_State *L = test_lua_open();

	luaL_openlibs(L);

	lua_register(L, "aux_getinfo_short_src", aux_getinfo_short_src);

	assert_int_equal(luaL_dostring(L, chunk_getinfo_short_src[0]), 0);

	assert_int_equal(luaL_dostring(L, chunk_getinfo_short_src[1]), 0);

	lua_close(L);
}

/* Yep, syntax in the chunk below is intentional, please don't break. */
static const char *chunk_getinfo_long_src =
	" -- Lorem ipsum dolor sit amet, consectetur adipiscing elit,   "
	"    sed do eiusmod tempor incididunt ut labore et dolore magna "
	"    aliqua. Ut enim ad minim veniam, quis nostrud exercitation "
	"    ullamco laboris nisi ut aliquip ex ea commodo consequat.   "
	"    Duis aute irure dolor in reprehenderit in voluptate velit  "
	"    esse cillum dolore eu fugiat nulla pariatur. Excepteur     "
	"    sint occaecat cupidatat non proident, sunt in culpa qui    "
	"    officia deserunt mollit anim id est laborum.             \n"
	" aux_getinfo_long_src() " /* defined in C */
	;

static int aux_getinfo_long_src(lua_State *L)
{
	lua_Debug _ar = {0};
	lua_Debug *ar = &_ar;

	assert_int_equal(lua_getstack(L, 1, ar), 1);
	assert_int_not_equal(lua_getinfo(L, "S", ar), 0);

	assert_true(strlen(ar->short_src) == (size_t)(LUA_IDSIZE - 1));
	assert_non_null(strstr(ar->short_src, "[string \""));
	assert_non_null(strstr(ar->short_src, "...\"]"));

	return 0;
}

static void test_lua_getinfo_long_src(void **state)
{
	UNUSED(state);
	lua_State *L = test_lua_open();

	luaL_openlibs(L);
	lua_register(L, "aux_getinfo_long_src", aux_getinfo_long_src);
	assert_int_equal(luaL_dostring(L, chunk_getinfo_long_src), 0);
	lua_close(L);
}

/* Testing lua_getupvalue */

static const char *chunk_getupvalue = " local x = 'XX'        "
				      " local function foo(f) "
				      "     assert(x == 'XX') "
				      "     aux_getupvalue(f) "
				      " end                   "
				      " foo(foo)              ";

static int aux_getupvalue(lua_State *L)
{
	assert_stack_size(L, 1);
	assert_true(lua_isfunction(L, -1));

	/* Lua function, non-existent upvalue: */
	assert_ptr_equal(lua_getupvalue(L, -1, 100), NULL);
	assert_stack_size(L, 1);

	/* Lua function, real upvalue: */
	assert_string_equal(lua_getupvalue(L, -1, 1), "x");
	assert_stack_size(L, 2);
	test_string(L, -1, "XX");

	return 0;
}

static void test_lua_getupvalue(void **state)
{
	UNUSED(state);
	lua_State *L = test_lua_open();

	luaL_openlibs(L);

	lua_pushliteral(L, "C_GETUPVALUE");
	lua_pushcclosure(L, aux_getupvalue, 1);
	lua_setglobal(L, "aux_getupvalue");
	assert_stack_size(L, 0);

	assert_int_equal(luaL_dostring(L, chunk_getupvalue), 0);

	lua_getglobal(L, "aux_getupvalue");

	/* C function, non-existent upvalue: */
	assert_ptr_equal(lua_getupvalue(L, -1, 100), NULL);
	assert_stack_size(L, 1);

	/* C function, real upvalue: */
	assert_string_equal(lua_getupvalue(L, -1, 1), "");
	assert_stack_size(L, 2);
	test_string(L, -1, "C_GETUPVALUE");

	lua_close(L);
}

/* Testing lua_setupvalue */

static const char *chunk_setupvalue = " local x = 'XX'        "
				      " local function foo(f) "
				      "     assert(x == 'XX') "
				      "     aux_setupvalue(f) "
				      "     assert(x == 'YY') "
				      " end                   "
				      " foo(foo)              ";

static int aux_setupvalue(lua_State *L)
{
	assert_stack_size(L, 1);
	assert_true(lua_isfunction(L, 1));
	lua_pushliteral(L, "YY");

	/* non-existent upvalue: */
	assert_ptr_equal(lua_setupvalue(L, 1, 100), NULL);
	assert_stack_size(L, 2);

	/* real upvalue: */
	assert_string_equal(lua_setupvalue(L, 1, 1), "x");
	assert_stack_size(L, 1);
	return 0;
}

static void test_lua_setupvalue(void **state)
{
	UNUSED(state);
	lua_State *L = test_lua_open();

	luaL_openlibs(L);

	lua_pushliteral(L, "C_SETUPVALUE");
	lua_pushcclosure(L, aux_setupvalue, 1);
	lua_setglobal(L, "aux_setupvalue");
	assert_stack_size(L, 0);

	assert_int_equal(luaL_dostring(L, chunk_setupvalue), 0);

	lua_getglobal(L, "aux_setupvalue");
	lua_pushliteral(L, "c_setupvalue");

	/* C function, non-existent upvalue: */
	assert_ptr_equal(lua_setupvalue(L, 1, 100), NULL);
	assert_stack_size(L, 2);

	/* C function, real upvalue: */
	assert_string_equal(lua_setupvalue(L, 1, 1), "");
	assert_stack_size(L, 1);

	/* Assert that the upvalue was actually set: */
	assert_string_equal(lua_getupvalue(L, 1, 1), "");
	assert_stack_size(L, 2);
	test_string(L, -1, "c_setupvalue");

	lua_close(L);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_lua_getstack),
		cmocka_unit_test(test_lua_getlocal),
		cmocka_unit_test(test_lua_setlocal),
		cmocka_unit_test(test_lua_getinfo_long_src),
		cmocka_unit_test(test_lua_getinfo_short_src),
		cmocka_unit_test(test_lua_getupvalue),
		cmocka_unit_test(test_lua_setupvalue)};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
