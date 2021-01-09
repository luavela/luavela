/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common_lua.h"

#include "lj_obj.h"

#define VMSTATE_MAGIC ((int32_t)0xdeadbeef)

/* Hack into VM's internals and set VM state to some magic value. */
static void touch_vmstate(lua_State *L)
{
	G(L)->vmstate = VMSTATE_MAGIC;
}

/* Assert that VM state is correctly saved and restored by the interpreter. */
static void assert_vmstate(const lua_State *L)
{
	assert_int_equal(G(L)->vmstate, VMSTATE_MAGIC);
}

static lua_State *assert_test_init(const char *chunk)
{
	lua_State *L = test_lua_open();

	luaL_openlibs(L);
	assert_int_equal(luaL_dostring(L, chunk), 0);
	return L;
}

static void test_vmstate_call(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = assert_test_init("function payload() return true end");

	touch_vmstate(L);

	lua_getglobal(L, "payload");
	lua_call(L, 0, 0);

	assert_vmstate(L);

	lua_close(L);
}

static void test_vmstate_pcall_ok(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = assert_test_init("function payload() return true end");

	touch_vmstate(L);

	lua_getglobal(L, "payload");
	assert_int_equal(lua_pcall(L, 0, 0, 0), 0);

	assert_vmstate(L);

	lua_close(L);
}

static void test_vmstate_pcall_err(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = assert_test_init("function payload() error('ERR') end");

	touch_vmstate(L);

	lua_getglobal(L, "payload");
	assert_int_equal(lua_pcall(L, 0, 0, 0), LUA_ERRRUN);

	assert_vmstate(L);

	lua_close(L);
}

static int cpcall_ok(lua_State *L)
{
	const char *chunk = "function payload() return true end";

	assert_int_equal(luaL_dostring(L, chunk), 0);
	lua_getglobal(L, "payload");
	lua_call(L, 0, 0);
	return 0;
}

static void test_vmstate_cpcall_ok(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();

	touch_vmstate(L);

	assert_int_equal(lua_cpcall(L, cpcall_ok, NULL), 0);

	assert_vmstate(L);

	lua_close(L);
}

static int cpcall_err(lua_State *L)
{
	return luaL_error(L, "ERROR");
}

static void test_vmstate_cpcall_err(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();

	touch_vmstate(L);

	assert_int_equal(lua_cpcall(L, cpcall_err, NULL), LUA_ERRRUN);

	assert_vmstate(L);

	lua_close(L);
}

static void test_vmstate_resume_ok(void **state)
{
	UNUSED_STATE(state);

	const char *chunk = "function payload()            \n"
			    "    coroutine.yield('YIELD')  \n"
			    "    return 'FINISH'           \n"
			    "end                           \n";
	lua_State *L = assert_test_init(chunk);

	touch_vmstate(L);

	lua_getglobal(L, "payload");

	assert_int_equal(lua_resume(L, 0), LUA_YIELD);
	assert_vmstate(L);
	assert_stack_size(L, 1);
	test_string(L, -1, "YIELD");
	lua_pop(L, 1);

	assert_int_equal(lua_resume(L, 0), 0);
	assert_vmstate(L);
	assert_stack_size(L, 1);
	test_string(L, -1, "FINISH");
	lua_pop(L, 1);

	lua_close(L);
}

static void test_vmstate_resume_err(void **state)
{
	UNUSED_STATE(state);

	const char *chunk = "function payload()            \n"
			    "    coroutine.yield('YIELD')  \n"
			    "    error('ERROR')            \n"
			    "end                           \n";
	lua_State *L = assert_test_init(chunk);

	touch_vmstate(L);

	lua_getglobal(L, "payload");

	assert_int_equal(lua_resume(L, 0), LUA_YIELD);
	assert_vmstate(L);
	assert_stack_size(L, 1);
	test_string(L, -1, "YIELD");
	lua_pop(L, 1);

	assert_int_equal(lua_resume(L, 0), LUA_ERRRUN);
	assert_vmstate(L);
	test_substring(L, -1, "ERROR");

	lua_close(L);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_vmstate_call),
		cmocka_unit_test(test_vmstate_pcall_ok),
		cmocka_unit_test(test_vmstate_pcall_err),
		cmocka_unit_test(test_vmstate_cpcall_ok),
		cmocka_unit_test(test_vmstate_cpcall_err),
		cmocka_unit_test(test_vmstate_resume_ok),
		cmocka_unit_test(test_vmstate_resume_err),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
