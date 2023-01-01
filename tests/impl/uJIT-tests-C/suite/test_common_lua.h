/*
 * Common includes and definitions for uJIT unit tests
 * that cover core uJIT functionality.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJIT_UNIT_TESTS_COMMON_LUA_H_
#define _UJIT_UNIT_TESTS_COMMON_LUA_H_

#include <string.h>

#include "test_common.h"

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "lextlib.h"

#define UJ_UNIT_AINLINE inline __attribute__((always_inline))

static UJ_UNIT_AINLINE void assert_stack_size(lua_State *L, int expected_size)
{
	assert_int_equal(lua_gettop(L), expected_size);
}

/* Create a new Lua state and run some trivial integrity checks */
static UJ_UNIT_AINLINE lua_State *test_lua_open()
{
	lua_State *L = lua_open();

	assert_non_null(L);
	assert_stack_size(L, 0);
	return L;
}

static UJ_UNIT_AINLINE void test_nil(lua_State *L, int idx)
{
	assert_true(lua_isnil(L, idx));
}

static UJ_UNIT_AINLINE void test_boolean(lua_State *L, int idx, int b)
{
	assert_true(b == 0 || b == 1); /* as per lua_toboolean in the RefMan */
	assert_true(lua_isboolean(L, idx));
	assert_int_equal(lua_toboolean(L, idx), b);
}

/* Test that a number object inside Lua carries expected payload */
static UJ_UNIT_AINLINE void test_number(lua_State *L, int idx, lua_Number n)
{
	assert_true(lua_isnumber(L, idx));
	assert_double_equal(lua_tonumber(L, idx), n, 1E-8);
}

static UJ_UNIT_AINLINE void test_integer(lua_State *L, int idx, lua_Integer n)
{
	assert_true(lua_isnumber(L, idx));
	assert_int_equal(lua_tointeger(L, idx), n);
}

/* Test that a string object inside Lua carries expected payload */
static UJ_UNIT_AINLINE void test_string(lua_State *L, int idx, const char *str)
{
	size_t len = 0;

	assert_true(lua_isstring(L, idx));
	assert_string_equal(lua_tolstring(L, idx, &len), str);
	assert_int_equal(len, strlen(str));
}

static UJ_UNIT_AINLINE void test_global_string(lua_State *L, const char *name,
					       const char *value)
{
	lua_getglobal(L, name);
	test_string(L, -1, value);
	lua_pop(L, 1);
}

static UJ_UNIT_AINLINE void test_substring(lua_State *L,
					   int idx, const char *expected)
{
	size_t len = 0;

	assert_true(lua_isstring(L, idx));
	assert_true(strstr(lua_tolstring(L, idx, &len), expected) != NULL);
}

static UJ_UNIT_AINLINE void test_enforce_gc_cycle(lua_State *L)
{
	/* Enforce GC to start as soon as possible: */
	lua_gc(L, LUA_GCSETPAUSE, 10);
	/* Enforce GC to collect as much as possible during a cycle: */
	lua_gc(L, LUA_GCSETSTEPMUL, 1000);
	/* ...and apply the new settings: */
	lua_gc(L, LUA_GCRESTART, -1);
}

#endif /* !_UJIT_UNIT_TESTS_COMMON_LUA_H_ */
