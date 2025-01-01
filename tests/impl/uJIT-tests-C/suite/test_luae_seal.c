/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common_lua.h"

static int aux_pcall_seal(lua_State *L)
{
	assert_stack_size(L, 1);
	luaE_seal(L, -1);
	return 0;
}

static void assert_nyi_function(lua_State *L, const char *chunk, const char *fn)
{
	assert_stack_size(L, 0);
	assert_int_equal(luaL_dostring(L, chunk), 0);
	assert_stack_size(L, 0);
	lua_pushcfunction(L, aux_pcall_seal);
	lua_getglobal(L, fn);
	assert_true(lua_isfunction(L, -1));
	assert_int_equal(lua_pcall(L, 1, 0, 0), LUA_ERRRUN);
	test_substring(L, -1, "attempt to seal a function with upvalues");
	lua_pop(L, 1);
}

static void test_seal_nyi_types(void **state)
{
	UNUSED_STATE(state);

	const size_t SIZE_OF_UDATA = 1024;
	const char *chunk_fn_uv = "local upvalue = 'foo'\n"
				  "function fn_uv() return upvalue end";

	lua_State *L = test_lua_open();

	/* userdata */
	assert_stack_size(L, 0);
	lua_pushcfunction(L, aux_pcall_seal);
	assert_non_null(lua_newuserdata(L, SIZE_OF_UDATA));
	assert_int_equal(lua_pcall(L, 1, 0, 0), LUA_ERRRUN);
	test_substring(L, -1, "object of unsupported type");
	lua_pop(L, 1);

	/* coroutine */
	assert_stack_size(L, 0);
	lua_pushcfunction(L, aux_pcall_seal);
	assert_non_null(lua_newthread(L));
	assert_int_equal(lua_pcall(L, 1, 0, 0), LUA_ERRRUN);
	test_substring(L, -1, "object of unsupported type");
	lua_pop(L, 1);

	/* function that depends on upvalues */
	assert_nyi_function(L, chunk_fn_uv, "fn_uv");

	lua_close(L);
}

/* rawset(t, "foo", "bar") */
static int aux_mutator_rawset(lua_State *L)
{
	assert_stack_size(L, 1);
	assert_true(lua_istable(L, 1));
	lua_pushstring(L, "foo");
	lua_pushstring(L, "bar");
	lua_rawset(L, 1);
	return 0;
}

/* rawset(t, 1, "foo") */
static int aux_mutator_rawseti(lua_State *L)
{
	assert_stack_size(L, 1);
	assert_true(lua_istable(L, 1));
	lua_pushstring(L, "foo");
	lua_rawseti(L, 1, 1);
	return 0;
}

/* t.foo = "bar" -- with lua_setfield */
static int aux_mutator_setfield(lua_State *L)
{
	assert_stack_size(L, 1);
	assert_true(lua_istable(L, 1));
	lua_pushstring(L, "bar");
	lua_setfield(L, 1, "foo");
	return 0;
}

/* t.foo = "bar" -- with lua_settable */
static int aux_mutator_settable(lua_State *L)
{
	assert_stack_size(L, 1);
	assert_true(lua_istable(L, 1));
	lua_pushstring(L, "foo");
	lua_pushstring(L, "bar");
	lua_settable(L, 1);
	return 0;
}

/* setmetatable(t, mt) */
static int aux_mutator_setmetatable(lua_State *L)
{
	assert_stack_size(L, 1);
	assert_true(lua_istable(L, 1));
	lua_newtable(L);
	lua_setmetatable(L, 1);
	return 0;
}

static void assert_modification(lua_State *L, lua_CFunction mutator)
{
	assert_stack_size(L, 1);
	lua_pushcfunction(L, mutator);
	lua_pushvalue(L, -2);
	assert_int_equal(lua_pcall(L, 1, 0, 0), LUA_ERRRUN);
	test_substring(L, -1, "attempt to modify");
	lua_pop(L, 1);
}

static void assert_modifications(lua_State *L, int assert_setmetatable)
{
	/*
	 * Mutator: lua_setglobal
	 * As per the RefMan, implemented through lua_setfield, so see below.
	 */

	assert_modification(L, aux_mutator_rawset);
	assert_modification(L, aux_mutator_rawseti);
	assert_modification(L, aux_mutator_setfield);
	assert_modification(L, aux_mutator_settable);

	if (assert_setmetatable)
		assert_modification(L, aux_mutator_setmetatable);
}

static void test_seal_modification(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();

	/* local t = ujit.seal({}) */
	lua_newtable(L);
	luaE_seal(L, -1);

	assert_modifications(L, 1);

	lua_close(L);
}

static void test_seal_modification_with_metatable(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();

	/* local t = setmetatable(ujit.seal({}), {}) */
	lua_newtable(L);
	lua_newtable(L);
	lua_setmetatable(L, 1);
	luaE_seal(L, -1);

	assert_modifications(L, 0);

	lua_close(L);
}

static void test_seal_modification_recursive(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();

	/* local t = {}; t.t = t */
	lua_newtable(L);
	lua_pushstring(L, "t");
	lua_pushvalue(L, 1);
	lua_settable(L, 1);

	/* assert(t == t.t) */
	assert_stack_size(L, 1);
	lua_getfield(L, 1, "t");
	assert_true(lua_rawequal(L, 1, 2));
	lua_pop(L, 1);

	/* ujit.seal(t) */
	assert_stack_size(L, 1);
	luaE_seal(L, -1);

	assert_modifications(L, 1);

	lua_close(L);
}

static void test_seal_atomic(void **state)
{
	UNUSED_STATE(state);

	const size_t level = 3;
	size_t i;
	lua_State *L = test_lua_open();

	/*
	 * Create a table with nested subtables, put a coroutine at the
	 * inner-most level, try to seal the entire structure and fail.
	 */

	lua_newtable(L);

	for (i = 0; i < level; i++) {
		lua_pushstring(L, "nested");
		lua_newtable(L);
	}

	lua_pushstring(L, "oops");
	assert_non_null(lua_newthread(L));
	lua_settable(L, -3);

	for (i = 0; i < level; i++)
		lua_settable(L, -3);

	assert_stack_size(L, 1);
	lua_pushcfunction(L, aux_pcall_seal);
	lua_pushvalue(L, 1);
	assert_int_equal(lua_pcall(L, 1, 0, 0), LUA_ERRRUN);
	test_substring(L, -1, "object of unsupported type");
	lua_pop(L, 1);

	/* Write at each nesting level to ensure that the table is writable. */

	assert_stack_size(L, 1);
	i = 0;
	lua_getfield(L, 1, "nested");
	while (!lua_isnil(L, -1)) {
		i++;
		assert_true(lua_istable(L, -1));
		lua_pushstring(L, "bar");
		lua_setfield(L, -2, "foo");
		lua_getfield(L, -1, "nested");
		lua_remove(L, -2);
	}

	assert_int_equal(i, level);
	assert_stack_size(L, 2);

	lua_close(L);
}

static void test_seal_consistent(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();

	/* local t1 = { oops = coroutine.create(...) } */
	lua_newtable(L);
	assert_non_null(lua_newthread(L));
	lua_setfield(L, 1, "oops");

	/* local t2 = ujit.seal({}) */
	lua_newtable(L);
	luaE_seal(L, -1);

	/* local t3 = t1; t3.sealed = t2; */
	lua_pushvalue(L, 1);
	lua_pushvalue(L, 2);
	lua_setfield(L, -2, "sealed");

	/* clear redundant slots: */
	assert_stack_size(L, 3);
	lua_pop(L, 2);

	/* pcall(ujit.seal, t1) -- failure expected */
	lua_pushcfunction(L, aux_pcall_seal);
	lua_pushvalue(L, 1);
	assert_int_equal(lua_pcall(L, 1, 0, 0), LUA_ERRRUN);
	test_substring(L, -1, "object of unsupported type");
	lua_pop(L, 1);

	/* t1.foo = "bar" -- t1 is still writable */
	assert_stack_size(L, 1);
	lua_pushstring(L, "bar");
	lua_setfield(L, 1, "foo");

	/* t1.sealed is still sealed: */

	assert_stack_size(L, 1);
	lua_getfield(L, 1, "sealed");
	lua_remove(L, 1);

	assert_stack_size(L, 1);
	assert_modifications(L, 1);

	lua_close(L);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_seal_nyi_types),
		cmocka_unit_test(test_seal_modification),
		cmocka_unit_test(test_seal_modification_with_metatable),
		cmocka_unit_test(test_seal_modification_recursive),
		cmocka_unit_test(test_seal_atomic),
		cmocka_unit_test(test_seal_consistent),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
