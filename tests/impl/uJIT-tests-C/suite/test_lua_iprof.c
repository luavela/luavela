/*
 * A set of tests for iprof related features.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <unistd.h>
#include "test_common_lua.h"

const char *ujit_iprof_profile =
	" return function (pfn, pcb, name, mode)                               "
	"        return function(...)                                          "
	"                local s, e = ujit.iprof.start(name, mode)             "
	"                local t = { pfn(...) }                                "
	"                if s then pcb(ujit.iprof.stop()) else pcb(nil, e) end "
	"                return table.unpack(t)                                "
	"        end                                                           "
	" end                                                                  ";

static void assert_ujit_iprof_profile(lua_State *L)
{
	lua_getglobal(L, "ujit");
	assert_true(lua_istable(L, -1));
	lua_getfield(L, -1, "iprof");
	assert_true(lua_istable(L, -1));
	lua_getfield(L, -1, "profile");
	assert_true(lua_isnil(L, -1));
	lua_pop(L, 1);
	assert_int_equal(luaL_dostring(L, ujit_iprof_profile), 0);
	assert_true(lua_isfunction(L, -1) && !lua_iscfunction(L, -1));
	lua_setfield(L, -2, "profile");
	lua_pop(L, 2);
}

static void assert_report_table(lua_State *L, const char *metric)
{
	int count;

	assert_stack_size(L, 1);
	assert_true(lua_istable(L, -1));
	lua_pushnil(L);
	assert_true(lua_next(L, -2));
	assert_string_equal(metric, lua_tostring(L, -2));
	assert_true(lua_istable(L, -1));
	for (count = 0, lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1), count++)
		assert_true(lua_isnumber(L, -1));
	assert_int_equal(count, 2);
	/* No remaining elements */
	lua_pop(L, 1);
	assert_false(lua_next(L, -2));
}

const char *simple_name = "SIMPLE";
const char *simple_chunk =
	" local __ = 0                                                        "
	" local s, e = ujit.iprof.start(simple_name, ujit.iprof.PLAIN)        "
	" for _ = 1, 1000000 do __ = __ + _ end                               "
	" if not s then simple_cb(s, e) else simple_cb(ujit.iprof.stop()) end ";

static int simple_cb(lua_State *L)
{
	assert_report_table(L, simple_name);
	/* test simple */
	lua_getfield(L, -1, simple_name);
	lua_getfield(L, -1, "lua");
	lua_getfield(L, -2, "wall");
	assert_double_equal(lua_tonumber(L, -2), lua_tonumber(L, -1), 1e-9);
	return 0;
}

static void test_simple(void **state)
{
	UNUSED(state);

	lua_State *L = test_lua_open();

	/* Initialize Lua VM */
	luaL_openlibs(L);
	/* Register callback for reporting and entity name */
	lua_register(L, "simple_cb", simple_cb);
	lua_pushstring(L, simple_name);
	lua_setglobal(L, "simple_name");
	assert_int_equal(luaL_dostring(L, simple_chunk), 0);
	lua_close(L);
}

const char *more_name = "MORE";
const char *more_chunk =
	" local __ = 0                                                    "
	" local s, e = ujit.iprof.start(more_name, ujit.iprof.PLAIN)      "
	" for _ = 1, 1000000 do __ = __ + _ end                           "
	" coroutine.yield()                                               "
	" for _ = 1, 1000000 do __ = __ + _ end                           "
	" if not s then more_cb(s, e) else more_cb(ujit.iprof.stop()) end ";

static int more_cb(lua_State *L)
{
	assert_report_table(L, more_name);
	/* test more */
	lua_getfield(L, -1, more_name);
	lua_getfield(L, -1, "lua");
	lua_getfield(L, -2, "wall");
	assert_double_equal(lua_tonumber(L, -2) + 1, lua_tonumber(L, -1), 1e0);
	return 0;
}

static void test_more(void **state)
{
	UNUSED(state);

	lua_State *L = test_lua_open();

	/* Initialize Lua VM */
	luaL_openlibs(L);

	lua_State *L1 = lua_newthread(L);

	assert_non_null(L1);
	/* Register callback for reporting and entity name */
	lua_register(L1, "more_cb", more_cb);
	lua_pushstring(L1, more_name);
	lua_setglobal(L1, "more_name");
	assert_int_equal(luaL_loadstring(L1, more_chunk), 0);
	assert_int_equal(lua_resume(L1, 0), LUA_YIELD);
	assert_int_equal(sleep(1), 0);
	assert_int_equal(lua_resume(L1, 0), 0);

	lua_close(L);
}

int fibonacci_count;
const char *fibonacci_name = "FIBONACCI";
const char *fibonacci_chunk =
	" return ujit.iprof.profile(function (n)               "
	"        local function f(m)                           "
	"                assert(m >= 0)                        "
	"                if m < 2 then                         "
	"                       return m                       "
	"                else                                  "
	"                       coroutine.yield()              "
	"                       return f(m - 1) + f(m - 2)     "
	"                end                                   "
	"        end                                           "
	"        return f(n)                                   "
	" end, fibonacci_cb, fibonacci_name, ujit.iprof.PLAIN) ";

static int fibonacci_cb(lua_State *L)
{
	assert_report_table(L, fibonacci_name);
	/* test fibonacci */
	lua_getfield(L, -1, fibonacci_name);
	lua_getfield(L, -1, "lua");
	lua_getfield(L, -2, "wall");
	assert_double_equal(lua_tonumber(L, -2) + fibonacci_count,
			    lua_tonumber(L, -1), 1e0);
	return 0;
}

static void test_fibonacci(void **state)
{
	UNUSED(state);

	int status;
	lua_State *L = test_lua_open();

	/* Initialize Lua VM */
	luaL_openlibs(L);
	/* Check whether ujit.iprof.profile was not defined by luaL_openlibs */
	assert_ujit_iprof_profile(L);

	lua_State *L1 = lua_newthread(L);

	assert_non_null(L1);
	/* Register callback for reporting and entity name */
	lua_register(L1, "fibonacci_cb", fibonacci_cb);
	lua_pushstring(L1, fibonacci_name);
	lua_setglobal(L1, "fibonacci_name");
	assert_int_equal(luaL_dostring(L1, fibonacci_chunk), 0);
	lua_pushnumber(L1, 5);
	for (fibonacci_count = 0; (status = lua_resume(L1, 1)) == LUA_YIELD;
	     fibonacci_count++)
		assert_int_equal(sleep(1), 0);
	assert_int_equal(status, 0);
	assert_int_equal(fibonacci_count, 7);
	assert_int_equal(lua_tonumber(L1, -1), 5);

	lua_close(L);
}

int factorial_count;
const char *factorial_name = "FACTORIAL";
const char *factorial_chunk =
	" function factorial(n)                                             "
	"         if n == 0 then                                            "
	"                 return 1                                          "
	"         else                                                      "
	"                 coroutine.yield()                                 "
	"                 return n * factorial(n - 1)                       "
	"         end                                                       "
	" end                                                               "
	" return ujit.iprof.profile(                                        "
	"         factorial, factorial_cb, factorial_name, ujit.iprof.PLAIN "
	" )                                                                 ";

static int factorial_cb(lua_State *L)
{
	assert_report_table(L, factorial_name);
	/* test factorial */
	lua_getfield(L, -1, factorial_name);
	lua_getfield(L, -1, "lua");
	lua_getfield(L, -2, "wall");
	assert_double_equal(lua_tonumber(L, -2) + factorial_count,
			    lua_tonumber(L, -1), 1e0);
	return 0;
}

static void test_factorial(void **state)
{
	UNUSED(state);

	int status;
	lua_State *L = test_lua_open();

	/* Initialize Lua VM */
	luaL_openlibs(L);
	/* Check whether ujit.iprof.profile was not defined by luaL_openlibs */
	assert_ujit_iprof_profile(L);

	lua_State *L1 = lua_newthread(L);

	assert_non_null(L1);
	/* Register callback for reporting and entity name */
	lua_register(L1, "factorial_cb", factorial_cb);
	lua_pushstring(L1, factorial_name);
	lua_setglobal(L1, "factorial_name");
	assert_int_equal(luaL_dostring(L1, factorial_chunk), 0);
	lua_pushnumber(L1, 5);
	for (factorial_count = 0; (status = lua_resume(L1, 1)) == LUA_YIELD;
	     factorial_count++)
		assert_int_equal(sleep(1), 0);
	assert_int_equal(status, 0);
	assert_int_equal(factorial_count, 5);
	assert_int_equal(lua_tonumber(L1, -1), 120);

	lua_close(L);
}

const char *input_name = "INPUT";
/* Chunk is proposed to validate behaviour with typos made in arguments passed */
const char *plane_chunk = " ujit.iprof.start('P', ujit.iprof.PLANE) ";
/* Chunk is proposed to validate behaviour with invalid arguments passed */
const char *string_chunk = " ujit.iprof.start('I', input_name) ";

static void test_input(void **state)
{
	UNUSED(state);

	lua_State *L = test_lua_open();

	/* Initialize Lua VM */
	luaL_openlibs(L);
	/* Register and entity name */
	lua_pushstring(L, input_name);
	lua_setglobal(L, "input_name");
	assert_int_equal(luaL_dostring(L, plane_chunk), 0);
	assert_int_equal(luaL_dostring(L, string_chunk), 1);

	lua_close(L);
}

const char *memory_name = "MEMORY";
const char *memory_chunk =
	" local __ = 0                                                   "
	" local iterations = 512                                         "
	" local t, e                                                     "
	" for _ = 1, iterations do                                       "
	"         t, e = ujit.iprof.start(memory_name, ujit.iprof.PLAIN) "
	"         if t then __ = __ + 1 end                              "
	" end                                                            "
	" assert(__ == iterations)                                       "
	" for _ = 1, iterations do                                       "
	"         t, e = ujit.iprof.stop()                               "
	"         if t then __ = __ + 1 end                              "
	" end                                                            "
	" assert(__ == iterations)                                       ";

static void test_memory(void **state)
{
	UNUSED(state);

	lua_State *L = test_lua_open();

	/* Initialize Lua VM */
	luaL_openlibs(L);
	/* Register and entity name */
	lua_pushstring(L, memory_name);
	lua_setglobal(L, "memory_name");
	assert_int_equal(luaL_dostring(L, memory_chunk), 0);

	lua_close(L);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_simple),
		cmocka_unit_test(test_more),
		cmocka_unit_test(test_fibonacci),
		cmocka_unit_test(test_factorial),
		cmocka_unit_test(test_input),
		cmocka_unit_test(test_memory),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
