/*
 * A set of tests for iprof related features.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <time.h>
#include <unistd.h>
#include "test_common_lua.h"

#define EPSILON 1e-6

/* Clock emulation {{{ */

/*
 * We try to make the test less fragile by removing its dependence on the system
 * time by overriding <clock_gettime(3)> function. For this reason, clock mock
 * static counter is introduced and <clock_gettime> simply adds a jiffy to this
 * counter, normalizes .nsec and .sec fields of the mock and respectively sets
 * the value to the given struct timespec argument.
 *
 * According to jargon file (http://www.catb.org/~esr/jargon/html/J/jiffy.html):
 * | 1. The duration of one tick of the system clock on your computer. Often one
 * |    AC cycle time (1/60 second in the U.S. and Canada, 1/50 most other
 * |    places), but more recently 1/100 sec has become common. "The swapper
 * |    runs every 6 jiffies" means that the virtual memory management routine
 * |    is executed once for every 6 ticks of the clock, or about ten times a
 * |    second.
 * | 2. Confusingly, the term is sometimes also used for a 1-millisecond wall
 * |    time interval.
 * | 3. Even more confusingly, physicists semi-jokingly use 'jiffy' to mean the
 * |    time required for light to travel one foot in a vacuum, which turns out
 * |    to be close to one nanosecond. Other physicists use the term for the
 * |    quantum-mechanical lower bound on meaningful time lengths.
 * | 4. Indeterminate time from a few seconds to forever. "I'll do it in a
 * |    jiffy" means certainly not now and possibly never. This is a bit
 * |    contrary to the more widespread use of the word. Oppose nano.
 *
 * Besides, there are several <sleep(3)> calls required in the test cases to
 * check whether "lua" and "wall" counters are calculated right. This function
 * is also overridden, considering how time is counted within this test: since
 * <sleep> receives the seconds as an argument, simply add the given amount of
 * seconds to the clock mock.
 *
 * Unfortunately, there is one flaw in such time mocking: these are not real
 * nanoseconds and seconds used in the clock mock, but just two types of
 * counters: short (for consecutive actions) and long (for interrupted actions).
 * Hence, we can't use some real jiffy value (i.e. the first one from the jargon
 * file), considering the cumulative discrepancy between linear mocked clock
 * behaviour and non-linear real clock behaviour.
 *
 * Considering everything above "physicists semi-joking" constant (i.e. 1 ns)
 * looks fine to be a jiffy in for a clock mock.
 */
#define JIFFY (1L)
#define NSECNORM ((long)1e9)

static struct timespec clock_mock = {
	.tv_sec = 0,
	.tv_nsec = 0,
};

extern int clock_gettime(clockid_t clockid, struct timespec *tp)
{
	UNUSED(clockid);
	clock_mock.tv_nsec += JIFFY;
	clock_mock.tv_sec += clock_mock.tv_nsec / NSECNORM;
	clock_mock.tv_nsec = clock_mock.tv_nsec % NSECNORM;
	tp->tv_sec = clock_mock.tv_sec;
	tp->tv_nsec = clock_mock.tv_nsec;
	return 0;
}

extern unsigned int sleep(unsigned int seconds)
{
	clock_mock.tv_sec += seconds;
	return 0;
}

/* }}} */

const char *ujit_iprof_profile =
	" return function (pfn, pcb, name, mode, level)                   "
	"   return function(...)                                          "
	"     local s, e = ujit.iprof.start(name, mode, level)            "
	"     local t = { pfn(...) }                                      "
	"     if s then pcb(ujit.iprof.stop()) else pcb(nil, e) end       "
	"     return table.unpack(t)                                      "
	"   end                                                           "
	" end                                                             ";

static void assert_ujit_iprof_profile(lua_State *L)
{
	lua_getglobal(L, "ujit");
	assert_true(lua_istable(L, -1));
	lua_getfield(L, -1, "iprof");
	assert_true(lua_istable(L, -1));
	lua_getfield(L, -1, "profile");
	test_nil(L, -1);
	lua_pop(L, 1);
	assert_int_equal(luaL_dostring(L, ujit_iprof_profile), 0);
	assert_true(lua_isfunction(L, -1) && !lua_iscfunction(L, -1));
	lua_setfield(L, -2, "profile");
	lua_pop(L, 2);
}

typedef void (*assert_report_field)(lua_State *L);

static void assert_report_subs(lua_State *L);
static void assert_report_number(lua_State *L);

static assert_report_field asserts[] = {
	['c'] = assert_report_number, /* calls */
	['l'] = assert_report_number, /* lua */
	['w'] = assert_report_number, /* wall */
	['s'] = assert_report_subs, /* subs */
};

static void assert_report_stats(lua_State *L)
{
	assert_true(lua_istable(L, -1));

	for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
		assert_true(lua_isstring(L, -2));
		asserts[(int)*lua_tostring(L, -2)](L);
	}
}

static void assert_report_subs(lua_State *L)
{
	for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
		assert_true(lua_isstring(L, -2));
		assert_report_stats(L);
	}
}

static void assert_report_number(lua_State *L)
{
	assert_true(lua_isnumber(L, -1));
	assert_true(lua_tonumber(L, -1) > 0);
}

static void assert_report_table(lua_State *L, const char *report_name)
{
	assert_stack_size(L, 1);
	assert_true(lua_istable(L, -1));

	lua_getfield(L, -1, report_name);

	assert_report_stats(L);

	/* Pop report table to restore initial stack state */
	lua_pop(L, 1);
}

#define lua_export(L, type, variable) \
	(lua_push##type(L, variable), lua_setglobal(L, #variable))

const char *simple_name = "SIMPLE";
const char *simple_chunk =
	" jit.off()                                                           "
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
	assert_double_equal(lua_tonumber(L, -2), lua_tonumber(L, -1), EPSILON);
	lua_pop(L, 2);
	lua_getfield(L, -1, "calls");
	test_integer(L, -1, 1);
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
	lua_export(L, string, simple_name);
	assert_int_equal(luaL_dostring(L, simple_chunk), 0);
	lua_close(L);
}

const char *more_name = "MORE";
const char *more_chunk =
	" jit.off()                                                       "
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
	assert_double_equal(lua_tonumber(L, -2) + 1, lua_tonumber(L, -1),
			    EPSILON);
	lua_pop(L, 2);
	lua_getfield(L, -1, "calls");
	test_integer(L, -1, 1);
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
	lua_export(L1, string, more_name);
	assert_int_equal(luaL_loadstring(L1, more_chunk), 0);
	assert_int_equal(lua_resume(L1, 0), LUA_YIELD);
	assert_int_equal(sleep(1), 0);
	assert_int_equal(lua_resume(L1, 0), 0);

	lua_close(L);
}

int fibonacci_count;
const char *fibonacci_name = "FIBONACCI";
const char *fibonacci_chunk =
	" jit.off()                                            "
	" return ujit.iprof.profile(function (n)               "
	"   local function f(m)                                "
	"     assert(m >= 0)                                   "
	"     if m < 2 then                                    "
	"       return m                                       "
	"     else                                             "
	"       coroutine.yield()                              "
	"       return f(m - 1) + f(m - 2)                     "
	"     end                                              "
	"   end                                                "
	"   return f(n)                                        "
	" end, fibonacci_cb, fibonacci_name, ujit.iprof.PLAIN) ";

static int fibonacci_cb(lua_State *L)
{
	assert_report_table(L, fibonacci_name);
	/* test fibonacci */
	lua_getfield(L, -1, fibonacci_name);
	lua_getfield(L, -1, "lua");
	lua_getfield(L, -2, "wall");
	assert_double_equal(lua_tonumber(L, -2) + fibonacci_count,
			    lua_tonumber(L, -1), EPSILON);
	lua_pop(L, 2);
	lua_getfield(L, -1, "calls");
	test_integer(L, -1, 1);
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
	lua_export(L1, string, fibonacci_name);
	assert_int_equal(luaL_dostring(L1, fibonacci_chunk), 0);
	lua_pushnumber(L1, 5);
	for (fibonacci_count = 0; (status = lua_resume(L1, 1)) == LUA_YIELD;
	     fibonacci_count++)
		assert_int_equal(sleep(1), 0);
	assert_int_equal(status, 0);
	assert_int_equal(fibonacci_count, 7);
	test_integer(L1, -1, 5);

	lua_close(L);
}

int factorial_count;
const char *factorial_name = "FACTORIAL";
const char *factorial_chunk =
	" jit.off()                                                   "
	" function factorial(n)                                       "
	"   if n == 0 then                                            "
	"     return 1                                                "
	"   else                                                      "
	"     coroutine.yield()                                       "
	"     return n * factorial(n - 1)                             "
	"   end                                                       "
	" end                                                         "
	" return ujit.iprof.profile(                                  "
	"   factorial, factorial_cb, factorial_name, ujit.iprof.PLAIN "
	" )                                                           ";

static int factorial_cb(lua_State *L)
{
	assert_report_table(L, factorial_name);
	/* test factorial */
	lua_getfield(L, -1, factorial_name);
	lua_getfield(L, -1, "lua");
	lua_getfield(L, -2, "wall");
	assert_double_equal(lua_tonumber(L, -2) + factorial_count,
			    lua_tonumber(L, -1), EPSILON);
	lua_pop(L, 2);
	lua_getfield(L, -1, "calls");
	test_integer(L, -1, 1);
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
	lua_export(L1, string, factorial_name);
	assert_int_equal(luaL_dostring(L1, factorial_chunk), 0);
	lua_pushnumber(L1, 5);
	for (factorial_count = 0; (status = lua_resume(L1, 1)) == LUA_YIELD;
	     factorial_count++)
		assert_int_equal(sleep(1), 0);
	assert_int_equal(status, 0);
	assert_int_equal(factorial_count, 5);
	test_integer(L1, -1, 120);

	lua_close(L);
}

unsigned int parent_iter = 5, parent_timeout = 1;
const char *parent_name = "PARENT";
const char *parent_chunk =
	" jit.off() "
	" local parent = ujit.iprof.profile(function(iter, timeout) "
	"   local coro = coroutine.create(function(n, t)            "
	"     for _ = 1, n do                                       "
	"       sleep(t)                                            "
	"       coroutine.yield()                                   "
	"     end                                                   "
	"   end)                                                    "
	"   local ok = coroutine.resume(coro, iter, timeout)        "
	"   while ok and coroutine.status(coro) ~= 'dead' do        "
	"     ok = coroutine.resume(coro)                           "
	"   end                                                     "
	" end, parent_cb, parent_name, ujit.iprof.PLAIN)            "
	" parent(parent_iter, parent_timeout)                       ";

static int sleep_lua(lua_State *L)
{
	sleep(luaL_checkint(L, 1));
	return 0;
}

static int parent_cb(lua_State *L)
{
	assert_report_table(L, parent_name);
	/* test parent */
	lua_getfield(L, -1, parent_name);
	lua_getfield(L, -1, "lua");
	lua_getfield(L, -2, "wall");
	assert_double_equal(lua_tonumber(L, -2) + parent_iter * parent_timeout,
			    lua_tonumber(L, -1), EPSILON);
	lua_pop(L, 2);
	lua_getfield(L, -1, "calls");
	test_integer(L, -1, 1);
	return 0;
}

static void test_parent(void **state)
{
	UNUSED(state);

	lua_State *L = test_lua_open();

	/* Initialize Lua VM */
	luaL_openlibs(L);
	/* Check whether ujit.iprof.profile was not defined by luaL_openlibs */
	assert_ujit_iprof_profile(L);

	/* Register function Lua C wrapper for sleep(3) function */
	lua_register(L, "sleep", sleep_lua);
	/* Register callback for reporting and entity name */
	lua_register(L, "parent_cb", parent_cb);
	lua_export(L, string, parent_name);
	lua_export(L, number, parent_iter);
	lua_export(L, number, parent_timeout);
	assert_int_equal(luaL_dostring(L, parent_chunk), 0);

	lua_close(L);
}

const char *metamethod_name = "METAMETHOD";
const char *metamethod_chunk =
	" jit.off()                                                       "
	" local Q = setmetatable({}, {                                    "
	"   __index = function(self, index) return metamethod_default end "
	" })                                                              "
	" local qq = ujit.iprof.profile(function (key)                    "
	"   return Q[key]                                                 "
	" end, metamethod_cb, metamethod_name, ujit.iprof.PLAIN)          "
	" return qq('QKRQ')                                               ";

static int metamethod_cb(lua_State *L)
{
	assert_report_table(L, metamethod_name);
	/* test metamethod */
	lua_getfield(L, -1, metamethod_name);
	lua_getfield(L, -1, "lua");
	lua_getfield(L, -2, "wall");
	assert_double_equal(lua_tonumber(L, -2), lua_tonumber(L, -1), EPSILON);
	lua_pop(L, 2);
	lua_getfield(L, -1, "calls");
	test_integer(L, -1, 1);
	return 0;
}

static void test_metamethod(void **state)
{
	UNUSED(state);

	int metamethod_default = 42;
	lua_State *L = test_lua_open();

	/* Initialize Lua VM */
	luaL_openlibs(L);
	/* Check whether ujit.iprof.profile was not defined by luaL_openlibs */
	assert_ujit_iprof_profile(L);

	/* Register callback for reporting and entity name */
	lua_register(L, "metamethod_cb", metamethod_cb);
	lua_export(L, string, metamethod_name);
	lua_export(L, number, metamethod_default);
	assert_int_equal(luaL_dostring(L, metamethod_chunk), 0);
	test_integer(L, -1, metamethod_default);

	lua_close(L);
}

const char *cframe_name = "CFRAME";
const char *cframe_chunk =
	" jit.off()                                         "
	" local wcfunc = ujit.iprof.profile(                "
	"   cfunc, cframe_cb, cframe_name, ujit.iprof.PLAIN "
	" )                                                 "
	" return wcfunc(function(p) return p end)           ";

static int cfunc(lua_State *L)
{
	lua_pushnumber(L, 42);
	lua_call(L, 1, 1);
	return 1;
}

static int cframe_cb(lua_State *L)
{
	assert_report_table(L, cframe_name);
	/* test cframe */
	lua_getfield(L, -1, cframe_name);
	lua_getfield(L, -1, "lua");
	lua_getfield(L, -2, "wall");
	assert_double_equal(lua_tonumber(L, -2), lua_tonumber(L, -1), EPSILON);
	lua_pop(L, 2);
	lua_getfield(L, -1, "calls");
	test_integer(L, -1, 1);
	return 0;
}

static void test_cframe(void **state)
{
	UNUSED(state);

	lua_State *L = test_lua_open();

	/* Initialize Lua VM */
	luaL_openlibs(L);
	/* Check whether ujit.iprof.profile was not defined by luaL_openlibs */
	assert_ujit_iprof_profile(L);

	/* Register callback for reporting and entity name */
	lua_register(L, "cframe_cb", cframe_cb);
	lua_export(L, string, cframe_name);
	lua_register(L, "cfunc", cfunc);
	assert_int_equal(luaL_dostring(L, cframe_chunk), 0);
	test_integer(L, -1, 42);

	lua_close(L);
}

const char *ffi_name = "FFI";
const char *ffi_chunk =
	" jit.off()                                               "
	" local ffi = require 'ffi'                               "
	" ffi.cdef('int strcmp(const char *s1, const char *s2);') "
	" char_p = ffi.typeof('char *')                           "
	/* strcmp */
	" local strcmp = ujit.iprof.profile(function(s1, s2)      "
	"   return tonumber(ffi.C.strcmp(                         "
	"                                ffi.cast(char_p, s1),    "
	"                                ffi.cast(char_p, s2)     "
	"                               ))                        "
	" end, ffi_cb, ffi_name, ujit.iprof.EXCLUSIVE)            "
	" return strcmp(s1, s2)                                   ";

static int ffi_cb(lua_State *L)
{
	assert_report_table(L, ffi_name);
	/* test ffi */
	lua_getfield(L, -1, ffi_name);
	lua_getfield(L, -1, "lua");
	lua_getfield(L, -2, "wall");
	assert_double_equal(lua_tonumber(L, -2), lua_tonumber(L, -1), EPSILON);
	lua_pop(L, 2);
	lua_getfield(L, -1, "calls");
	test_integer(L, -1, 1);
	return 0;
}

static void test_ffi(void **state)
{
	UNUSED(state);

	const char *s1 = "v5.16.3", *s2 = "v5.30.0";
	lua_State *L = test_lua_open();

	/* Initialize Lua VM */
	luaL_openlibs(L);
	/* Check whether ujit.iprof.profile was not defined by luaL_openlibs */
	assert_ujit_iprof_profile(L);

	/* Register callback for reporting and entity name */
	lua_register(L, "ffi_cb", ffi_cb);
	lua_export(L, string, ffi_name);
	lua_export(L, string, s1);
	lua_export(L, string, s2);
	assert_int_equal(luaL_dostring(L, ffi_chunk), 0);
	assert_true(lua_tonumber(L, -1) < 0);

	lua_close(L);
}

const char *hook_name = "HOOK";
const char *hook_chunk =
	" jit.off()                                                          "
	" function h(event) local _ = 0 for __ = 1, 1e3 do _ = _ + 1 end end "
	" local foo = ujit.iprof.profile(function(n)                         "
	"   local __ = 0                                                     "
	"   for _ = 1, n do __ = __ + _ end                                  "
	"   return __                                                        "
	" end, hook_cb, hook_name, ujit.iprof.INCLUSIVE)                     "
	" debug.sethook(h, 'cr')                                             "
	" return foo(42)                                                     ";

static int hook_cb(lua_State *L)
{
	assert_report_table(L, hook_name);
	/* test hook */
	lua_getfield(L, -1, hook_name);
	lua_getfield(L, -1, "lua");
	lua_getfield(L, -2, "wall");
	assert_double_equal(lua_tonumber(L, -2), lua_tonumber(L, -1), EPSILON);
	lua_pop(L, 2);
	lua_getfield(L, -1, "calls");
	test_integer(L, -1, 1);
	return 0;
}

static void test_hook(void **state)
{
	UNUSED(state);

	lua_State *L = test_lua_open();

	/* Initialize Lua VM */
	luaL_openlibs(L);
	/* Check whether ujit.iprof.profile was not defined by luaL_openlibs */
	assert_ujit_iprof_profile(L);

	/* Register callback for reporting and entity name */
	lua_register(L, "hook_cb", hook_cb);
	lua_export(L, string, hook_name);
	assert_int_equal(luaL_dostring(L, hook_chunk), 0);
	test_integer(L, -1, 903);

	lua_close(L);
}

int limit_count;
const char *limit_name = "LIMIT";
const char *limit_chunk =
	" jit.off()                                                     "
	" return ujit.iprof.profile(function (n)                        "
	"   local function f(m)                                         "
	"     assert(m >= 0)                                            "
	"     if m < 2 then                                             "
	"       return m                                                "
	"     else                                                      "
	"       coroutine.yield()                                       "
	"       return f(m - 1) + f(m - 2)                              "
	"     end                                                       "
	"   end                                                         "
	"   return f(n)                                                 "
	" end, limit_cb, limit_name, ujit.iprof.INCLUSIVE, limit_level) ";

static int limit_cb(lua_State *L)
{
	assert_report_table(L, limit_name);
	/* test limit */
	lua_getfield(L, -1, limit_name);
	lua_getfield(L, -1, "lua");
	lua_getfield(L, -2, "wall");
	assert_double_equal(lua_tonumber(L, -2) + limit_count,
			    lua_tonumber(L, -1), EPSILON);
	lua_pop(L, 2);
	lua_getfield(L, -1, "calls");
	test_integer(L, -1, 1);
	return 0;
}

static void test_limit(void **state)
{
	UNUSED(state);

	int status, limit_level = 5;
	lua_State *L = test_lua_open();

	/* Initialize Lua VM */
	luaL_openlibs(L);
	/* Check whether ujit.iprof.profile was not defined by luaL_openlibs */
	assert_ujit_iprof_profile(L);

	lua_State *L1 = lua_newthread(L);

	assert_non_null(L1);
	/* Register callback for reporting and entity name */
	lua_register(L1, "limit_cb", limit_cb);
	lua_export(L1, string, limit_name);
	lua_export(L1, number, limit_level);
	assert_int_equal(luaL_dostring(L1, limit_chunk), 0);
	lua_pushnumber(L1, 9);
	for (limit_count = 0; (status = lua_resume(L1, 1)) == LUA_YIELD;
	     limit_count++)
		assert_int_equal(sleep(1), 0);
	assert_int_equal(status, 0);
	assert_int_equal(limit_count, 54);
	test_integer(L1, -1, 34);

	lua_close(L);
}

const char *die_name = "DIE";
const char *die_chunk = " jit.off()                                        "
			" local function croak(message) error(message) end "
			" local die = ujit.iprof.profile(function(iter)    "
			"   local __ = 0                                   "
			"   for _ = 1, iter do                             "
			"     __ = __ + _                                  "
			"     if _ == iter then croak('QQ') end            "
			"   end                                            "
			" end, die_cb, die_name, ujit.iprof.PLAIN)         "
			" pcall(die, 1000)                                 ";

static int die_cb(lua_State *L)
{
	UNUSED(L);

	lua_assert(0);
	/* Unreachable */
	return 0;
}

static void test_die(void **state)
{
	UNUSED(state);

	lua_State *L = test_lua_open();

	/* Initialize Lua VM */
	luaL_openlibs(L);
	/* Check whether ujit.iprof.profile was not defined by luaL_openlibs */
	assert_ujit_iprof_profile(L);

	/* Register callback for reporting and entity name */
	lua_register(L, "die_cb", die_cb);
	lua_export(L, string, die_name);
	assert_int_equal(luaL_dostring(L, die_chunk), 0);
	lua_close(L);
}

const char *unwind_name = "UNWIND";
const char *unwind_chunk = " jit.off()                                        "
			   " local function croak(message) error(message) end "
			   " local unwind = ujit.iprof.profile(function(iter) "
			   "   local __ = 0                                   "
			   "   for _ = 1, iter do                             "
			   "     __ = __ + _                                  "
			   "     if _ == iter then pcall(croak, 'QQ') end     "
			   "   end                                            "
			   " end, unwind_cb, unwind_name, ujit.iprof.PLAIN)   "
			   " unwind(10)                                       ";

static int unwind_cb(lua_State *L)
{
	assert_report_table(L, unwind_name);
	/* test unwind */
	lua_getfield(L, -1, unwind_name);
	lua_getfield(L, -1, "lua");
	lua_getfield(L, -2, "wall");
	assert_double_equal(lua_tonumber(L, -2), lua_tonumber(L, -1), EPSILON);
	lua_pop(L, 2);
	lua_getfield(L, -1, "calls");
	test_integer(L, -1, 1);
	return 0;
}

static void test_unwind(void **state)
{
	UNUSED(state);

	lua_State *L = test_lua_open();

	/* Initialize Lua VM */
	luaL_openlibs(L);
	/* Check whether ujit.iprof.profile was not defined by luaL_openlibs */
	assert_ujit_iprof_profile(L);

	/* Register callback for reporting and entity name */
	lua_register(L, "unwind_cb", unwind_cb);
	lua_export(L, string, unwind_name);
	assert_int_equal(luaL_dostring(L, unwind_chunk), 0);
	lua_close(L);
}

const char *input_name = "INPUT";
/* Chunk is proposed to validate behaviour with typos made in arguments passed */
const char *plane_chunk = " jit.off() ujit.iprof.start('P', ujit.iprof.PLANE) ";
/* Chunk is proposed to validate behaviour with invalid arguments passed */
const char *string_chunk = " jit.off() ujit.iprof.start('I', input_name) ";

static void test_input(void **state)
{
	UNUSED(state);

	lua_State *L = test_lua_open();

	/* Initialize Lua VM */
	luaL_openlibs(L);
	/* Register and entity name */
	lua_export(L, string, input_name);
	assert_int_equal(luaL_dostring(L, plane_chunk), 0);
	assert_int_equal(luaL_dostring(L, string_chunk), 1);

	lua_close(L);
}

const char *jit_on_name = "JIT ON";
const char *jit_on_chunk = " jit.off()                     "
			   " ujit.iprof.start(jit_on_name) "
			   " jit.on()                      ";

static void test_jit_on(void **state)
{
	UNUSED(state);

	lua_State *L = test_lua_open();
	/* Initialize Lua VM */
	luaL_openlibs(L);
	/* Register and entity name */
	lua_export(L, string, jit_on_name);
	assert_int_equal(luaL_dostring(L, jit_on_chunk), 1);
	test_substring(L, -1, "profiling cannot be proceeded with enabled JIT");

	lua_close(L);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_simple),
		cmocka_unit_test(test_more),
		cmocka_unit_test(test_fibonacci),
		cmocka_unit_test(test_factorial),
		cmocka_unit_test(test_parent),
		cmocka_unit_test(test_metamethod),
		cmocka_unit_test(test_cframe),
		cmocka_unit_test(test_ffi),
		cmocka_unit_test(test_hook),
		cmocka_unit_test(test_limit),
		cmocka_unit_test(test_die),
		cmocka_unit_test(test_unwind),
		cmocka_unit_test(test_input),
		cmocka_unit_test(test_jit_on),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
