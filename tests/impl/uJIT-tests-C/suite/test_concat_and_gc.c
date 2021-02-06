/*
 * Test cases:
 *  * GC interrupts concatenation and triggers stack reallocation
 *  * GC interrupts __concat metamethod and triggers stack reallocation
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common_lua.h"

static int gc_called_capi;
static int gc_called_vm;
static int gc_called_capi_mm;
static int gc_called_vm_mm;

/*
 * Payload for userdata __gc metamethods. The main goal is
 * to utilise the stack to trigger its reallocation.
 */
static int gc_payload(lua_State *L, int *gc_called)
{
	const lua_Integer nslots = 10000;
	const int nargs = lua_gettop(L);
	lua_Integer i = nslots;

	while (i)
		lua_pushinteger(L, i--);

	lua_pop(L, (int)nslots);

	assert_stack_size(L, nargs);
	assert_int_equal(*gc_called, 0);
	*gc_called = 1;
	return 0;
}

/* Create a dummy userdata and set its metatable with the __gc method. */
static void userdata_create(lua_State *L, lua_CFunction mm_gc)
{
	const size_t SIZE_OF_UDATA = 1024;
	const int nargs = lua_gettop(L);
	void *userdata = lua_newuserdata(L, SIZE_OF_UDATA);

	assert_non_null(userdata);
	memset(userdata, 0, SIZE_OF_UDATA);

	/* setmetatable(userdata, {__gc = mm_gc}) */
	lua_createtable(L, 0, 16);
	lua_pushcfunction(L, mm_gc);
	lua_setfield(L, -2, "__gc");
	lua_setmetatable(L, -2);

	/* Immediately pop from stack to make userdata collectable */
	lua_pop(L, 1);
	assert_stack_size(L, nargs);
}

static void test_finalize(lua_State *L, int *gc_called)
{
	assert_stack_size(L, 1);
	test_string(L, 1, "xxxyyy");
	assert_int_equal(*gc_called, 1);
	lua_close(L);
}

/* GC check during concatenation done via C API */

static int userdata_gc_capi(lua_State *L)
{
	return gc_payload(L, &gc_called_capi);
}

static void test_concat_and_gc_capi(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();

	lua_pushstring(L, "xxx");
	lua_pushstring(L, "yyy");
	userdata_create(L, userdata_gc_capi);

	test_enforce_gc_cycle(L);

	lua_concat(L, 2);

	test_finalize(L, &gc_called_capi);
}

/* GC check during concatenation done from Lua */

static int userdata_gc_vm(lua_State *L)
{
	return gc_payload(L, &gc_called_vm);
}

static void test_concat_and_gc_vm(void **state)
{
	UNUSED_STATE(state);

	const char *chunk = "function foo(a, b) return a .. b end";
	lua_State *L = test_lua_open();

	assert_int_equal(luaL_dostring(L, chunk), 0);
	assert_stack_size(L, 0);

	/* Prepare a Lua call foo("xxx", "yyy"): */
	lua_getfield(L, LUA_GLOBALSINDEX, "foo");
	assert_true(lua_isfunction(L, 1));
	lua_pushstring(L, "xxx");
	lua_pushstring(L, "yyy");
	userdata_create(L, userdata_gc_vm);

	test_enforce_gc_cycle(L);

	lua_call(L, 2, 1);

	test_finalize(L, &gc_called_vm);
}

/*
 * GC check during __concat metamethod call
 */

static int mm_cat(lua_State *L)
{
	assert_stack_size(L, 2);
	assert_true(lua_istable(L, 1));
	assert_true(lua_istable(L, 2));
	lua_pushstring(L, "y");
	lua_pushstring(L, "yy");
	test_enforce_gc_cycle(L);
	lua_concat(L, 2);
	return 1;
}

static void prepare_stack_for_mm(lua_State *L, lua_CFunction mm_gc)
{
	/*
	 * local str1 = "xxx"
	 * local tab1 = {}
	 * setmetatable(tab1, {__concat = mm_cat})
	 * local tab2 = {}
	 */
	const int nargs = lua_gettop(L);

	lua_pushstring(L, "xxx");
	lua_createtable(L, 0, 16);
	lua_createtable(L, 0, 16);
	lua_pushcfunction(L, mm_cat);
	lua_setfield(L, -2, "__concat");
	lua_setmetatable(L, -2);
	lua_createtable(L, 0, 16);

	assert_int_equal(lua_gettop(L) - nargs, 3);

	userdata_create(L, mm_gc);
}

/* GC check during __concat metamethod call (from C API) */

static int userdata_gc_capi_mm(lua_State *L)
{
	return gc_payload(L, &gc_called_capi_mm);
}

static void test_concat_and_gc_capi_mm(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();

	prepare_stack_for_mm(L, userdata_gc_capi_mm);

	lua_concat(L, 3);

	test_finalize(L, &gc_called_capi_mm);
}

/* GC check during __concat metamethod call (from Lua) */

static int userdata_gc_vm_mm(lua_State *L)
{
	return gc_payload(L, &gc_called_vm_mm);
}

static void test_concat_and_gc_vm_mm(void **state)
{
	UNUSED_STATE(state);

	const char *chunk = "function foo(s, t1, t2) return s .. t1 .. t2 end";
	lua_State *L = test_lua_open();

	assert_int_equal(luaL_dostring(L, chunk), 0);
	assert_stack_size(L, 0);

	/* Prepare a Lua call foo("xxx", {}, {}): */
	lua_getfield(L, LUA_GLOBALSINDEX, "foo");
	assert_true(lua_isfunction(L, 1));
	prepare_stack_for_mm(L, userdata_gc_vm_mm);

	lua_call(L, 3, 1);

	test_finalize(L, &gc_called_vm_mm);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_concat_and_gc_capi),
		cmocka_unit_test(test_concat_and_gc_vm),
		cmocka_unit_test(test_concat_and_gc_capi_mm),
		cmocka_unit_test(test_concat_and_gc_vm_mm),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
