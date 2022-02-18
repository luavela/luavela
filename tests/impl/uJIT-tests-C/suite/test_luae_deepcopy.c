/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common_lua.h"

#include "uj_sbuf.h"
#include "lj_obj.h"

static void create_function(lua_State *L, const char *chunk)
{
	/*
	 * 1. create a generating function for the chunk, prepending "return "
	 * 2. load resulting code
	 * 3. set dummy fenv (for function that should be 'pure')
	 * 4. call generating function to get initially desired function
	 */
	struct sbuf sb;

	uj_sbuf_init(L, &sb);
	uj_sbuf_push_cstr(&sb, "return ");
	uj_sbuf_push_cstr(&sb, chunk);
	uj_sbuf_push_char(&sb, '\0');

	assert_true(luaL_loadstring(L, uj_sbuf_front(&sb)) == 0);

	lua_newtable(L);
	assert_true(lua_setfenv(L, -2) == 1);

	assert_true(lua_pcall(L, 0, 1, 0) == 0);
	assert_true(lua_isfunction(L, -1));
	assert_false(luaE_usesfenv(L, -1));

	uj_sbuf_free(L, &sb);
}

static void assert_function_result(lua_State *L, int idx)
{
	lua_getfield(L, idx, "func");
	lua_pushstring(L, "world");
	lua_call(L, 1, 1);
	lua_pushstring(L, "Hello, world60000!");
	assert_true(lua_equal(L, -1, -2));
	lua_remove(L, -1); /* pop reference string */
	lua_remove(L, -1); /* pop returned string */
}

static void push_error_message(lua_State *L, int idx)
{
	lua_getfield(L, idx, "func");
	lua_newtable(L);
	assert_true(lua_pcall(L, 1, 1, 0) == LUA_ERRRUN);
}

static void assert_error_message(lua_State *L1, int idx1, lua_State *L2,
				 int idx2)
{
	push_error_message(L1, idx1);
	push_error_message(L2, idx2);
	assert_string_equal(lua_tolstring(L1, -1, NULL),
			    lua_tolstring(L2, -1, NULL));
	lua_remove(L1, -1);
	lua_remove(L2, -1);
}

static void assert_zero_tmpmarks(lua_State *L)
{
	const GCobj *obj = G(L)->gc.root;

	while (obj != NULL && !uj_obj_is_sealed(obj)) {
		assert_true(!lj_obj_has_mark(obj) || &obj->th == L);
		obj = gcnext(obj);
	}
}

static void assert_table_content(lua_State *L, int idx)
{
	int size = lua_gettop(L);

	lua_getfield(L, idx, "foo");
	test_string(L, -1, "bar");
	lua_remove(L, -1);

	lua_pushnumber(L, 2);
	lua_gettable(L, idx);
	test_string(L, -1, "bar");
	lua_remove(L, -1);

	lua_pushnumber(L, 42);
	lua_gettable(L, idx);
	test_string(L, -1, "baz");
	lua_remove(L, -1);

	lua_pushnumber(L, 1);
	lua_gettable(L, idx);
	test_boolean(L, -1, 1);
	lua_remove(L, -1);

	assert_function_result(L, idx);

	lua_getfield(L, idx, "self");
	assert_true(lua_equal(L, idx, -1));
	lua_remove(L, -1);

	lua_getfield(L, idx, "child");
	lua_getfield(L, -1, "parent");
	assert_true(lua_equal(L, idx, -1));
	lua_remove(L, -1);
	lua_getfield(L, -1, "self");
	assert_true(lua_equal(L, -2, -1));
	lua_remove(L, -1);
	lua_getfield(L, -1, "we");
	test_string(L, -1, "need to go deeper");
	lua_remove(L, -1);
	lua_remove(L, -1); /* pop 'child' table */

	assert_stack_size(L, size);
}

static void test_deepcopy(void **state)
{
	UNUSED_STATE(state);

	lua_State *L1 = test_lua_open();
	lua_State *L2 = test_lua_open();

	luaL_openlibs(L1);
	luaL_openlibs(L2);

	/* t = {foo = "bar", [2] = "bar", [42] = "baz", [1] = true} */
	lua_createtable(L1, 4, 8);
	assert_stack_size(L1, 1);

	lua_pushstring(L1, "bar");
	lua_setfield(L1, -2, "foo");
	assert_stack_size(L1, 1);

	lua_pushstring(L1, "bar");
	lua_rawseti(L1, -2, 2);
	assert_stack_size(L1, 1);

	lua_pushinteger(L1, 42);
	lua_pushstring(L1, "baz");
	lua_settable(L1, -3);
	assert_stack_size(L1, 1);

	lua_pushboolean(L1, 1);
	lua_rawseti(L1, -2, 1);
	assert_stack_size(L1, 1);

	/* t["func"] = function (a) return "Hello, " .. a .. 60000 .. "!" end */
	/* NB: 60000 doesn't fit into KSHORT bytecode and stored in GCproto */
	create_function(
		L1,
		"function (a) return \"Hello, \" .. a .. 60000 .. \"!\" end");
	lua_setfield(L1, -2, "func");
	assert_stack_size(L1, 1);

	/* t["self"] = t */
	lua_pushvalue(L1, -1);
	lua_setfield(L1, -2, "self");
	assert_stack_size(L1, 1);

	/*
	 * child = {we = "need to go deeper", parent = t}
	 * child["self"] = child
	 * t["child"] = child
	 */
	lua_newtable(L1);
	lua_pushstring(L1, "we");
	lua_pushstring(L1, "need to go deeper");
	lua_settable(L1, -3);
	assert_stack_size(L1, 2);
	lua_pushvalue(L1, -2);
	lua_setfield(L1, -2, "parent");
	assert_stack_size(L1, 2);
	lua_pushvalue(L1, -1);
	lua_setfield(L1, -2, "self");
	assert_stack_size(L1, 2);
	lua_setfield(L1, -2, "child");
	assert_stack_size(L1, 1);

	luaE_deepcopytable(L2, L1, -1);
	assert_stack_size(L2, 1);

	luaE_deepcopytable(L1, L1, -1);
	assert_stack_size(L1, 2);
	assert_zero_tmpmarks(L1);

	/* collect garbage to explode later in case of dangling pointers */
	lua_gc(L1, LUA_GCCOLLECT, 0);
	lua_gc(L2, LUA_GCCOLLECT, 0);

	assert_table_content(L1, 2);
	assert_error_message(L1, 1, L1, 2);
	assert_error_message(L1, 1, L2, 1);
	lua_close(L1);

	assert_table_content(L2, 1);
	lua_close(L2);
}

static void test_deepcopy_immutable(void **state)
{
	UNUSED_STATE(state);

	lua_State *L1 = test_lua_open();
	lua_State *L2 = test_lua_open();

	/* local t = {} */
	lua_createtable(L1, 0, 4);
	assert_stack_size(L1, 1);

	/* t.str = "foo" */
	lua_pushstring(L1, "foo");
	lua_setfield(L1, -2, "str");
	assert_stack_size(L1, 1);

	/* t.self = t */
	lua_pushvalue(L1, -1);
	lua_setfield(L1, -2, "self");
	assert_stack_size(L1, 1);

	/* t.func = function (a) return "Hello, " .. a .. 60000 .. "!" end */
	/* NB: 60000 doesn't fit into KSHORT bytecode and stored in GCproto */
	create_function(
		L1,
		"function (a) return \"Hello, \" .. a .. 60000 .. \"!\" end");
	lua_setfield(L1, -2, "func");
	assert_stack_size(L1, 1);

	/* t.tab = {} */
	lua_newtable(L1);
	lua_setfield(L1, -2, "tab");
	assert_stack_size(L1, 1);

	luaE_immutable(L1, -1);

	luaE_deepcopytable(L2, L1, -1);
	assert_stack_size(L2, 1);

	/* copy_of_t.tab = {} -- must not fail, the result is mutable */
	lua_newtable(L2);
	lua_setfield(L2, -2, "tab");
	assert_stack_size(L2, 1);

	luaE_immutable(L2, -1); /* must be able to make the copy immutable */

	lua_close(L1);
	lua_close(L2);
}

static void test_deepcopy_sealed(void **state)
{
	UNUSED_STATE(state);

	lua_State *L1 = test_lua_open();
	lua_State *L2 = test_lua_open();

	/* local t = {} */
	lua_createtable(L1, 0, 4);
	assert_stack_size(L1, 1);

	/* t.str = "foo" */
	lua_pushstring(L1, "foo");
	lua_setfield(L1, -2, "str");
	assert_stack_size(L1, 1);

	/* t.self = t */
	lua_pushvalue(L1, -1);
	lua_setfield(L1, -2, "self");
	assert_stack_size(L1, 1);

	/* t.func = function (a) return "Hello, " .. a .. 60000 .. "!" end */
	/* NB: 60000 doesn't fit into KSHORT bytecode and stored in GCproto */
	create_function(
		L1,
		"function (a) return \"Hello, \" .. a .. 60000 .. \"!\" end");
	lua_setfield(L1, -2, "func");
	assert_stack_size(L1, 1);

	/* t.tab = {} */
	lua_newtable(L1);
	lua_setfield(L1, -2, "tab");
	assert_stack_size(L1, 1);

	luaE_seal(L1, -1);

	luaE_deepcopytable(L2, L1, -1);
	assert_stack_size(L2, 1);

	/* copy_of_t.tab = {} -- must not fail, the result is mutable */
	lua_newtable(L2);
	lua_setfield(L2, -2, "tab");
	assert_stack_size(L2, 1);

	luaE_seal(L2, -1); /* must be able to seal the copy */

	lua_close(L1);
	lua_close(L2);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_deepcopy),
		cmocka_unit_test(test_deepcopy_immutable),
		cmocka_unit_test(test_deepcopy_sealed),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
