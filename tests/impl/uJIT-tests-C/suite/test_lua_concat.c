/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common_lua.h"

/* lua-ref-man-5.1: lua_concat(L, 0) pushes an empty string onto the stack */
static void test_lua_concat_emptystr(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();

	lua_concat(L, 0);
	assert_stack_size(L, 1);

	test_string(L, 1, "");

	lua_close(L);
}

/* lua-ref-man-5.1: lua_concat(L, 1) is a no-op */
static void test_lua_concat_nop(void **state)
{
	UNUSED_STATE(state);

	const char *str = "test string";
	lua_State *L = test_lua_open();

	lua_pushlstring(L, str, strlen(str));
	assert_stack_size(L, 1);

	lua_concat(L, 1);
	assert_stack_size(L, 1);

	test_string(L, 1, str);

	lua_close(L);
}

/* lua-ref-man-5.1: lua_concat(L, 1) is a no-op, table as an argument */
static void test_lua_concat_nop_table(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();

	lua_createtable(L, 0, 16);
	assert_stack_size(L, 1);

	lua_concat(L, 1);
	assert_stack_size(L, 1);
	assert_int_equal(lua_istable(L, 1), 1);

	lua_close(L);
}

/* lua-ref-man-5.1: lua_concat(L, n), n > 1, no metamethods, no coercion */
static void test_lua_concat_naive(void **state)
{
	UNUSED_STATE(state);

	const char *str1 = "test";
	const char *str2 = " ";
	const char *str3 = "string";
	const char *res = "test string";
	lua_State *L = test_lua_open();

	lua_pushlstring(L, str1, strlen(str1));
	lua_pushlstring(L, str2, strlen(str2));
	lua_pushlstring(L, str3, strlen(str3));
	assert_stack_size(L, 3);

	lua_concat(L, 3);
	assert_stack_size(L, 1);

	test_string(L, 1, res);

	lua_close(L);
}

/* lua-ref-man-5.1: lua_concat(L, n), n > 1, no metamethods, no coercion */
static void test_lua_concat_naive_2steps(void **state)
{
	UNUSED_STATE(state);

	const char *str1 = "test";
	const char *str2 = " ";
	const char *str3 = "string";
	const char *res1 = " string";
	const char *res2 = "test string";
	lua_State *L = test_lua_open();

	lua_pushlstring(L, str1, strlen(str1));
	lua_pushlstring(L, str2, strlen(str2));
	lua_pushlstring(L, str3, strlen(str3));
	assert_stack_size(L, 3);

	lua_concat(L, 2);
	assert_stack_size(L, 2);
	test_string(L, 2, res1);

	lua_concat(L, 2);
	assert_stack_size(L, 1);
	test_string(L, 1, res2);

	lua_close(L);
}

/* lua-ref-man-5.1: lua_concat(L, n), n > 1, no metamethods, with coercion */
static void test_lua_concat_naive_coercion(void **state)
{
	UNUSED_STATE(state);

	const char *str1 = "test";
	const char *str2 = " ";
	const char *res = "test 42";
	lua_State *L = test_lua_open();

	lua_pushlstring(L, str1, strlen(str1));
	lua_pushlstring(L, str2, strlen(str2));
	lua_pushnumber(L, (lua_Number)42);
	assert_stack_size(L, 3);

	lua_concat(L, 3);
	assert_stack_size(L, 1);

	test_string(L, 1, res);

	lua_close(L);
}

/*
 * function mm_handler__concat1(s, t)
 *     return s .. "string"
 * end
 */
int mm_handler__concat1(lua_State *L)
{
	const char *str = "string";

	lua_pushvalue(L, -2);
	lua_pushlstring(L, str, strlen(str));
	lua_concat(L, 2);
	return 1;
}

/*
 * function mm_handler__concat2(t, s)
 *     return " " .. s
 * end
 */
int mm_handler__concat2(lua_State *L)
{
	const char *str = " ";

	lua_pushlstring(L, str, strlen(str));
	lua_pushvalue(L, -2);
	lua_concat(L, 2);
	return 1;
}

/*
 * function mm_handler__concat3(t, s)
 *     return "test" .. s
 * end
 */
int mm_handler__concat3(lua_State *L)
{
	const char *str = "test";

	lua_pushlstring(L, str, strlen(str));
	lua_pushvalue(L, -2);
	lua_concat(L, 2);
	return 1;
}

void test_create_metatable(lua_State *L, lua_CFunction mm_handler__concat)
{
	/* local mt = { __concat = CFUNC(mm_handler__concat) } */
	lua_createtable(L, 0, 16);
	lua_pushcfunction(L, mm_handler__concat);
	lua_setfield(L, 1, "__concat");
}

/* lua-ref-man-5.1: lua_concat(L, n), n > 1, mt-{} at index -1. */
static void test_lua_concat_mm1(void **state)
{
	UNUSED_STATE(state);

	const char *str1 = "test";
	const char *str2 = " ";
	const char *res = "test string";
	lua_State *L = test_lua_open();

	/* local mt = { __concat = ... } */
	test_create_metatable(L, mm_handler__concat1);
	assert_stack_size(L, 1);

	/* local str1 = ... */
	lua_pushlstring(L, str1, strlen(str1));
	assert_stack_size(L, 2);

	/* local str2 = ... */
	lua_pushlstring(L, str2, strlen(str2));
	assert_stack_size(L, 3);

	/* local t = setmetatable({}, mt) */
	lua_createtable(L, 0, 16);
	lua_pushvalue(L, 1);
	lua_setmetatable(L, 4);
	assert_stack_size(L, 4);

	/* str1 .. str2 .. t */
	lua_concat(L, 3);
	assert_stack_size(L, 2);

	test_string(L, 2, res);

	lua_close(L);
}

/* lua-ref-man-5.1: lua_concat(L, n), n > 1, mt-{} at index -2. */
static void test_lua_concat_mm2(void **state)
{
	UNUSED_STATE(state);

	const char *str1 = "test";
	const char *str2 = "string";
	const char *res = "test string";
	lua_State *L = test_lua_open();

	/* local mt = { __concat = ... } */
	test_create_metatable(L, mm_handler__concat2);
	assert_stack_size(L, 1);

	/* local str1 = ... */
	lua_pushlstring(L, str1, strlen(str1));
	assert_stack_size(L, 2);

	/* local t = setmetatable({}, mt) */
	lua_createtable(L, 0, 16);
	lua_pushvalue(L, 1);
	lua_setmetatable(L, 3);
	assert_stack_size(L, 3);

	/* local str2 = ... */
	lua_pushlstring(L, str2, strlen(str2));
	assert_stack_size(L, 4);

	/* str1 .. t .. str2 */
	lua_concat(L, 3);
	assert_stack_size(L, 2);

	test_string(L, 2, res);

	lua_close(L);
}

/* lua-ref-man-5.1: lua_concat(L, n), n > 1, mt-{} at index -3. */
static void test_lua_concat_mm3(void **state)
{
	UNUSED_STATE(state);

	const char *str1 = " ";
	const char *str2 = "string";
	const char *res = "test string";
	lua_State *L = test_lua_open();

	/* local mt = { __concat = ... } */
	test_create_metatable(L, mm_handler__concat3);
	assert_int_equal(lua_gettop(L), 1);

	/* local t = setmetatable({}, mt) */
	lua_createtable(L, 0, 16);
	lua_pushvalue(L, 1);
	lua_setmetatable(L, 2);
	assert_stack_size(L, 2);

	/* local str1 = ... */
	lua_pushlstring(L, str1, strlen(str1));
	assert_stack_size(L, 3);

	/* local str2 = ... */
	lua_pushlstring(L, str2, strlen(str2));
	assert_stack_size(L, 4);

	/* t .. str1 .. str2 */
	lua_concat(L, 3);
	assert_stack_size(L, 2);

	test_string(L, 2, res);

	lua_close(L);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_lua_concat_emptystr),
		cmocka_unit_test(test_lua_concat_nop),
		cmocka_unit_test(test_lua_concat_nop_table),
		cmocka_unit_test(test_lua_concat_naive),
		cmocka_unit_test(test_lua_concat_naive_2steps),
		cmocka_unit_test(test_lua_concat_naive_coercion),
		cmocka_unit_test(test_lua_concat_mm1),
		cmocka_unit_test(test_lua_concat_mm2),
		cmocka_unit_test(test_lua_concat_mm3),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
