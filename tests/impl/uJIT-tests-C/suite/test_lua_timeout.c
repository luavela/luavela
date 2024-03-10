/*
 * A set of tests for timeout-related features.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <signal.h>
#include <sched.h>
#include <time.h>
#include <sys/time.h>
#include "test_common_lua.h"

static const struct timeval default_timeout = {
	.tv_sec = 0,
	.tv_usec = 100000 /* 0.1 sec */
};

/****************************** AUX CHUNKS ******************************/

/*
 * Chunk shared between several tests, represents a simple case when
 * coroutine gets stuck in a forever loop. Several timeout callbacks are
 * available (commented below).
 */
static const char *chunk_forever_loop =
	"jit.off()                                             \n"
	/*
	 * Callback that will be called in case of timeout. It is a long
	 * runner as well, so we make sure that the callback will not be
	 * erroneously interrupted by another timeout exception.
	 */
	"function timeout_callback(msg)                        \n"
	"        local do_print = true                         \n"
	"        local i = 0                                   \n"
	"        while do_print do                             \n"
	"                i = i + 1                             \n"
	"                if i > 1e4 then                       \n"
	"                        return 42, msg                \n"
	"                end                                   \n"
	"        end                                           \n"
	"end                                                   \n"
	/* Timeout callback which always throws an error explicitly: */
	"function timeout_callback_unsafe(msg)                 \n"
	"        error('Error in timeout callback')            \n"
	"end                                                   \n"
	/* Timeout callback which throws an error "unintentionally": */
	"function timeout_callback_bad_code(obj)               \n"
	"        return obj.this_obj_is_nil                    \n"
	"end                                                   \n"
	/* Payload function with a forerver loop, never returns normally: */
	"local function foo(x, y)                              \n"
	"        while true do                                 \n"
	"                x = x + y                             \n"
	"        end                                           \n"
	"        return x                                      \n"
	"end                                                   \n"
	/* Coroutine's body, expects 1 argument (number/integer): */
	"function coroutine_start(n)                           \n"
	"        local bar = foo(42, n)                        \n"
	"end                                                   \n";

/****************************** AUX FUNCTIONS ******************************/

/* Simple function that suspends current thread for sec seconds. */
static void aux_sleep(uint64_t sec)
{
	time_t wait_start = time(NULL);

	while ((time(NULL) - wait_start) <= sec)
		sched_yield();
}

/*
 * Simply takes its first string argument and calls a corresponding function.
 */
static int aux_function_caller(lua_State *L)
{
	assert_stack_size(L, 2);
	assert_int_equal(lua_isstring(L, 1), 1);
	lua_getfield(L, LUA_GLOBALSINDEX, lua_tostring(L, 1));
	assert_int_equal(lua_isfunction(L, -1), 1);
	lua_pushvalue(L, -2);
	lua_call(L, 1, 1);
	return 1;
}

/* Lua wrapper around aux_sleep with the same semantics. */
static int aux_sleep_lua(lua_State *L)
{
	assert_stack_size(L, 1);
	lua_Integer n = lua_isnumber(L, -1) ? lua_tointeger(L, -1) : 1;

	lua_pop(L, -1);
	aux_sleep((uint64_t)n);
	return 0;
}

/****************************** AUX ASSERTIONS ******************************/

/*
 * By convention, coroutine body is a global function called "coroutine_start"
 * which accepts a single argument.
 */
static void assert_coroutine_init(lua_State *L, const struct timeval *timeout,
				  lua_CFunction timeout_callback)
{
	assert_true(luaE_intresolvable(timeout));
	assert_int_equal(luaE_settimeout(L, timeout, 0), LUAE_TIMEOUT_SUCCESS);
	if (timeout_callback != NULL)
		assert_null(luaE_settimeoutf(L, timeout_callback));

	lua_getfield(L, LUA_GLOBALSINDEX, "coroutine_start");
	lua_pushinteger(L, (lua_Integer)10);
	assert_stack_size(L, 2);
}

/* Asserts timeout and expected number of arguments returned by callback. */
static void assert_coroutine_timeout(lua_State *L, int nres)
{
	assert_int_equal(lua_resume(L, 1), LUAE_TIMEOUT);
	assert_stack_size(L, nres);
}

static lua_State *assert_test_init(const char *chunk)
{
	lua_State *L = test_lua_open();

	luaL_openlibs(L);

	/* Initialize timer interrupts */
	assert_int_equal(luaE_intinit(SIGALRM), LUAE_INT_SUCCESS);
	/* Load the chunk */
	if (chunk != NULL)
		assert_int_equal(luaL_dostring(L, chunk), 0);

	return L;
}

static void assert_test_cleanup(lua_State *L)
{
	assert_int_equal(luaE_intterm(), LUAE_INT_SUCCESS);
	lua_close(L);
}

/*
 * Common runner for cases when a timeout without timeout function must
 * be processed normally.
 */
static void assert_timeout_no_callback(const char *chunk)
{
	lua_State *L = assert_test_init(chunk);
	lua_State *L1 = lua_newthread(L);

	lua_register(L1, "aux_function_caller", aux_function_caller);
	lua_register(L1, "aux_sleep_lua", aux_sleep_lua);

	assert_coroutine_init(L1, &default_timeout, NULL);
	assert_coroutine_timeout(L1, 0);

	/* Coroutine not running: Unable to set / reset timeout. */
	assert_int_equal(luaE_settimeout(L1, &default_timeout, 0),
			 LUAE_TIMEOUT_ERRABORT);

	assert_test_cleanup(L);
}

/* Common runner for cases when a timeout must be processed normally. */
static void assert_timeout_with_callback(lua_CFunction timeout_callback)
{
	lua_State *L = assert_test_init(chunk_forever_loop);
	lua_State *L1 = lua_newthread(L);

	assert_coroutine_init(L1, &default_timeout, timeout_callback);
	assert_coroutine_timeout(L1, 2);
	test_integer(L1, 1, 42);
	test_string(L1, 2, "ERROR: Timeout");

	assert_test_cleanup(L);
}

/* Common runner for cases when a timeout function throws an error. */
static void assert_timeout_call_back_error(lua_CFunction timeout_callback,
					   const char *err_msg)
{
	lua_State *L = assert_test_init(chunk_forever_loop);
	lua_State *L1 = lua_newthread(L);

	assert_coroutine_init(L1, &default_timeout, timeout_callback);
	assert_coroutine_timeout(L1, 1);
	test_substring(L1, 1, err_msg);
	assert_test_cleanup(L);
}

/****************************** CASES ******************************/

/*
 * CASE: Unable to set timeouts for the main coroutine.
 */

static void test_no_timeout_for_main_thread(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = assert_test_init(NULL);

	assert_int_equal(luaE_settimeout(L, &default_timeout, 0),
			 LUAE_TIMEOUT_ERRMAIN);
	assert_test_cleanup(L);
}

/*
 * CASE: Unable to set timeouts unless timer interrupts are ticking.
 */

static void test_no_timeout_without_ticks(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();
	lua_State *L1 = lua_newthread(L);

	assert_int_equal(luaE_settimeout(L1, &default_timeout, 0),
			 LUAE_TIMEOUT_ERRNOTICKS);

	lua_close(L);
}

/*
 * CASE: Coroutine must not reset its own timeout (unless forced).
 */

/*
 * Implementation of an unsuccessfull attempt to reset a timeout
 * for a coroutine which already has timeout set.
 */
static int body_no_timeout_overriding(lua_State *L)
{
	const struct timeval timeout = {.tv_sec = 4, .tv_usec = 0};
	assert_int_equal(luaE_settimeout(L, &timeout, 0),
			 LUAE_TIMEOUT_ERRRUNNING);
	return 0;
}

static void test_no_timeout_overriding(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = assert_test_init(NULL);
	lua_State *L1 = lua_newthread(L);

	assert_int_equal(luaE_settimeout(L1, &default_timeout, 0),
			 LUAE_TIMEOUT_SUCCESS);
	lua_pushcfunction(L1, body_no_timeout_overriding);
	assert_int_equal(lua_resume(L1, 0), 0);
	assert_test_cleanup(L);
}

static void test_no_timeout_badtime(void **state)
{
	UNUSED_STATE(state);

	const struct timeval bad_timeout1 = {.tv_sec = -1, .tv_usec = 0};
	const struct timeval bad_timeout2 = {.tv_sec = 0, .tv_usec = -1};
	lua_State *L = assert_test_init(NULL);
	lua_State *L1 = lua_newthread(L);

	assert_int_equal(luaE_settimeout(L1, NULL, 0), LUAE_TIMEOUT_ERRBADTIME);
	assert_int_equal(luaE_settimeout(L1, &bad_timeout1, 0),
			 LUAE_TIMEOUT_ERRBADTIME);
	assert_int_equal(luaE_settimeout(L1, &bad_timeout2, 0),
			 LUAE_TIMEOUT_ERRBADTIME);

	assert_test_cleanup(L);
}

/*
 * CASE: Ensure that timeout without callback function works normally.
 */

static void test_timeout_trivial_loop(void **state)
{
	UNUSED_STATE(state);

	assert_timeout_no_callback(chunk_forever_loop);
}

/*
 * CASE: Ensure that timeout checkpoints at BC_*FUNC* and BC_RET* work.
 */

/* Please keep a tail call in foo to avoid guest stack overflow. */
static const char *chunk_timeout_trivial_call_ret =
	"jit.off()                                             \n"
	/* Payload function with a forerver recursion, never returns: */
	"local function foo(x, y)                              \n"
	"        return foo(x + y, y)                          \n"
	"end                                                   \n"
	/* Coroutine's body, expects 1 argument (number/integer): */
	"function coroutine_start(n)                           \n"
	"        local bar = foo(42, n)                        \n"
	"end                                                   \n";

static void test_timeout_trivial_call_ret(void **state)
{
	UNUSED_STATE(state);

	assert_timeout_no_callback(chunk_timeout_trivial_call_ret);
}

/*
 * CASE: ASAP expiration on an unresolvable timeout.
 */

static const char *chunk_timeout_asap =
	"jit.off()                                             \n"
	"function coroutine_start()                            \n"
	"        assert(false)                                 \n"
	"end                                                   \n";

static void test_timeout_asap(void **state)
{
	UNUSED_STATE(state);

	const struct timeval timeout = {.tv_sec = 0, .tv_usec = 1};
	lua_State *L = assert_test_init(chunk_timeout_asap);
	lua_State *L1 = lua_newthread(L);

	assert_false(luaE_intresolvable(&timeout));
	assert_int_equal(luaE_settimeout(L1, &timeout, 0),
			 LUAE_TIMEOUT_SUCCESS);

	lua_getfield(L1, LUA_GLOBALSINDEX, "coroutine_start");
	assert_stack_size(L1, 1);

	assert_int_equal(lua_resume(L1, 0), LUAE_TIMEOUT);
	assert_stack_size(L1, 0);

	assert_test_cleanup(L);
}

/*
 * CASE: ASAP expiration on an unresolvable timeout, C function as a payload.
 */

int func_timeout_asap_c(lua_State *L)
{
	UNUSED(L);
	/* This is by design, the test should never reach this code. */
	assert_true(0);
	return 0;
}

static void test_timeout_asap_c(void **state)
{
	UNUSED_STATE(state);

	const struct timeval timeout = {.tv_sec = 0, .tv_usec = 1};
	lua_State *L = assert_test_init(chunk_timeout_asap);
	lua_State *L1 = lua_newthread(L);

	assert_false(luaE_intresolvable(&timeout));
	assert_int_equal(luaE_settimeout(L1, &timeout, 0),
			 LUAE_TIMEOUT_SUCCESS);

	lua_pushcfunction(L1, func_timeout_asap_c);
	assert_stack_size(L1, 1);

	assert_int_equal(lua_resume(L1, 0), LUAE_TIMEOUT);
	assert_stack_size(L1, 0);

	assert_test_cleanup(L);
}

/*
 * CASE: ASAP expiration on an unresolvable timeout with restart.
 */

static const char *chunk_timeout_asap_restart =
	"jit.off()                                             \n"
	"function coroutine_start()                            \n"
	"        aux_timeout_asap_restart()                    \n"
	"        assert(false)                                 \n"
	"end                                                   \n";

/* A function that restarts the timeout on the fly with a too small value. */
static int aux_timeout_asap_restart(lua_State *L)
{
	const struct timeval timeout = {.tv_sec = 0, .tv_usec = 1};

	assert_stack_size(L, 0);
	assert_false(luaE_intresolvable(&timeout));
	assert_int_equal(luaE_settimeout(L, &timeout, 1), LUAE_TIMEOUT_SUCCESS);

	return 0;
}

static void test_timeout_asap_restart(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = assert_test_init(chunk_timeout_asap_restart);
	lua_State *L1 = lua_newthread(L);

	lua_register(L1, "aux_timeout_asap_restart", aux_timeout_asap_restart);

	lua_getfield(L1, LUA_GLOBALSINDEX, "coroutine_start");
	assert_stack_size(L1, 1);

	assert_int_equal(lua_resume(L1, 0), LUAE_TIMEOUT);
	assert_stack_size(L1, 0);

	assert_test_cleanup(L);
}

/*
 * CASE: Ensure that walking a non-trivial guest stack during
 *       handling timeout exception is not a problem.
 */

/*
 * Chunk with a tricky stack: A long running vararg function is called from
 * a metamethod which is called from a C function (C-call boundary here!)
 * which is called from a Lua function which is called with a pcall inside
 * the coroutine body.
 */
static const char *chunk_timeout_tricky_stack =
	"jit.off()                                                           \n"
	"local function vararg(narg, ...)                                    \n"
	"        local arguments = {...}                                     \n"
	"        local sum = 0                                               \n"
	"        local i = 2 * narg                                          \n"
	"        while i > 0 do                                              \n"
	"                sum = sum                                           \n"
	"                        + (arguments[1] or 1)                       \n"
	"                        + (arguments[2] or 1)                       \n"
	"                i = i - 1                                           \n"
	"        end                                                         \n"
	"        aux_sleep_lua(1)                                            \n"
	"        return narg * sum                                           \n"
	"end                                                                 \n"
	"function metamethod(k)                                              \n"
	"        local mt = {                                                \n"
	"                __add = function(t1, t2)                            \n"
	"                        return vararg(2, t1.n, t2.n) * (t1.n + t2.n)\n"
	"                end,                                                \n"
	"        }                                                           \n"
	"        local t1 = setmetatable({ n = 10 * k }, mt)                 \n"
	"        local t2 = setmetatable({ n = 20 * k }, mt)                 \n"
	"        return t1 + t2                                              \n"
	"end                                                                 \n"
	"local function protected(n)                                         \n"
	"        local result = aux_function_caller('metamethod', n)         \n"
	"        return result + 100                                         \n"
	"end                                                                 \n"
	"function coroutine_start(n)                                         \n"
	"        local status = pcall(protected, n)                          \n"
	"        return status                                               \n"
	"end                                                                 \n";

static void test_timeout_tricky_stack(void **state)
{
	UNUSED_STATE(state);

	assert_timeout_no_callback(chunk_timeout_tricky_stack);
}

/*
 * CASE: Ensure that timeout will be caught if
 *       a coroutine keeps yielding long enough.
 */

/* Chunk that yields in a forever loop. */
static const char *chunk_timeout_with_yield =
	"jit.off()                            \n"
	"function coroutine_start(n)          \n"
	"        local S = 0                  \n"
	"        local i = 0                  \n"
	"        while true do                \n"
	"                S = S + i            \n"
	"                i = i + n            \n"
	"                coroutine.yield()    \n"
	"        end                          \n"
	"        return S                     \n"
	"end                                  \n";

static void test_timeout_with_yield(void **state)
{
	UNUSED_STATE(state);

	int yielded = 0;
	lua_State *L = assert_test_init(chunk_timeout_with_yield);
	lua_State *L1 = lua_newthread(L);

	assert_coroutine_init(L1, &default_timeout, NULL);

	while (lua_resume(L1, yielded ? 0 : 1) != LUAE_TIMEOUT)
		yielded = 1;

	assert_int_equal(lua_status(L1), LUAE_TIMEOUT);
	assert_stack_size(L1, 0);

	assert_test_cleanup(L);
}

/*
 * CASE: Even if a coroutine yielded, its timeout still must be ticking.
 */

/*
 * Chunk that yields immediately after being resume. Caller will sleep
 * long enough, and then will catch a timeout exception on attempt to
 * resume the coroutine once again.
 */
static const char *chunk_timeout_instant_yield =
	"jit.off()                            \n"
	"function coroutine_start(n)          \n"
	"        coroutine.yield()            \n"
	"        return n + 1                 \n"
	"end                                  \n";

static void test_timeout_instant_yield(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = assert_test_init(chunk_timeout_instant_yield);
	lua_State *L1 = lua_newthread(L);

	assert_coroutine_init(L1, &default_timeout, NULL);
	assert_int_equal(lua_resume(L1, 1), LUA_YIELD);

	/* Wait for loger than timeout value and try to resume the coroutine */
	aux_sleep(1);

	assert_coroutine_timeout(L1, 0);

	assert_test_cleanup(L);
}

/*
 * CASE: Timeout callback must be reached and executed (trivial case).
 */

static int cb_timeout_with_callback(lua_State *L)
{
	lua_pushinteger(L, 42);
	lua_pushstring(L, "ERROR: Timeout");
	return 2;
}

static void test_timeout_with_callback(void **state)
{
	UNUSED_STATE(state);

	assert_timeout_with_callback(cb_timeout_with_callback);
}

/*
 * CASE: Timeout callback must be able to call an arbitrary Lua function.
 */

static int cb_timeout_with_callback_to_lua(lua_State *L)
{
	const int nres = 2;

	lua_getfield(L, LUA_GLOBALSINDEX, "timeout_callback");
	lua_pushstring(L, "ERROR: Timeout");
	lua_call(L, 1, nres);
	return nres;
}

static void test_timeout_with_callback_to_lua(void **state)
{
	UNUSED_STATE(state);

	assert_timeout_with_callback(cb_timeout_with_callback_to_lua);
}

/*
 * CASE: Explicit error(...) in a timeout callback is processed normally.
 */

static int cb_timeout_callback_throws(lua_State *L)
{
	lua_getfield(L, LUA_GLOBALSINDEX, "timeout_callback_unsafe");
	lua_pushstring(L, "ERROR: Timeout");
	assert_int_equal(lua_pcall(L, 1, 1, 0), LUA_ERRRUN);
	return 1; /* Because always throws */
}

static void test_timeout_callback_throws(void **state)
{
	UNUSED_STATE(state);

	assert_timeout_call_back_error(cb_timeout_callback_throws,
				       "Error in timeout callback");
}

/*
 * CASE: Run-time error in a timeout callback is processed normally.
 */

static int cb_timeout_callback_has_bad_op(lua_State *L)
{
	lua_getfield(L, LUA_GLOBALSINDEX, "timeout_callback_bad_code");
	lua_pushnil(L);
	assert_int_equal(lua_pcall(L, 1, 1, 0), LUA_ERRRUN);
	return 1; /* Because always throws */
}

static void test_timeout_callback_has_bad_op(void **state)
{
	UNUSED_STATE(state);

	assert_timeout_call_back_error(cb_timeout_callback_has_bad_op,
				       "attempt to index local");
}

/*
 * CASE: Timeouts can be "prolonged" from the same coroutine.
 */

/* Chunk that calls a function which restart the timeout on the fly. */
static const char *chunk_timeout_restart =
	"jit.off()                            \n"
	"function coroutine_start(n)          \n"
	"        aux_timeout_restart()        \n"
	"        local S = 0                  \n"
	"        local i = 0                  \n"
	"        while true do                \n"
	"                S = S + i            \n"
	"                i = i + n            \n"
	"        end                          \n"
	"        return S                     \n"
	"end                                  \n";

static int cb_timeout_restart(lua_State *L)
{
	/*
	 * Either of the two below will fail. This is by design,
	 * as the test should never reach this code.
	 */
	assert_null(L);
	assert_non_null(L);
	return 0;
}

/* A function that waits for some time and restarts the timeout on the fly. */
static int aux_timeout_restart(lua_State *L)
{
	assert_stack_size(L, 0);

	aux_sleep(1);

	assert_ptr_equal(luaE_settimeoutf(L, NULL), cb_timeout_restart);
	assert_int_equal(luaE_settimeout(L, &default_timeout, 1),
			 LUAE_TIMEOUT_SUCCESS);

	return 0;
}

static void test_timeout_restart(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = assert_test_init(chunk_timeout_restart);
	lua_State *L1 = lua_newthread(L);
	const struct timeval timeout = {.tv_sec = 4, .tv_usec = 0};

	lua_register(L1, "aux_timeout_restart", aux_timeout_restart);
	assert_coroutine_init(L1, &timeout, cb_timeout_restart);
	assert_coroutine_timeout(L1, 0);

	assert_test_cleanup(L);
}

/*
 * CASE: Timeout tracking is not performed while in hooks.
 */

/*
 * Couroutine is resumed with a timeout, but calls a long running hook which
 * pauses timeout tracking. As a result, the chunk runs longer than expected.
 */
static const char *chunk_timeout_long_hook =
	"jit.off()                                  \n"
	"local function foo()                       \n"
	"        return                             \n"
	"end                                        \n"
	"function coroutine_start(n)                \n"
	"        debug.sethook(aux_sleep_hook, 'c') \n"
	"        foo()                              \n"
	"        debug.sethook()                    \n"
	"        return n                           \n"
	"end                                        \n";

static int aux_sleep_hook(lua_State *L)
{
	/* Called from a hook, so an attempt to reset timeout fails. */
	assert_int_equal(luaE_settimeout(L, &default_timeout, 1),
			 LUAE_TIMEOUT_ERRHOOK);
	aux_sleep(1);
	return 0;
}

static void test_timeout_long_hook(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = assert_test_init(chunk_timeout_long_hook);
	lua_State *L1 = lua_newthread(L);

	lua_register(L1, "aux_sleep_hook", aux_sleep_hook);
	assert_coroutine_init(L1, &default_timeout, NULL);
	assert_coroutine_timeout(L1, 0);

	assert_test_cleanup(L);
}

/*
 * CASE: Guest stack should be available for introspection
 *       from the timeout function.
 */

/*
 * Lua code is specially crafted: by the time debug.getinfo() is called,
 * save_PC is not implicitly executed by any bytecode from the execution path.
 */
static const char *chunk_introspection_in_handler =
	"jit.off()                                \n"
	"function timeout_callback(msg)           \n"
	"        local info = debug.getinfo(2)    \n"
	"        return msg .. info.currentline   \n"
	"end                                      \n"
	"function coroutine_start(n)              \n"
	"        local S = n                      \n"
	"        while true do                    \n"
	"                S = S + n                \n"
	"        end                              \n"
	"        return S                         \n"
	"end                                      \n";

static int cb_introspection_in_handler(lua_State *L)
{
	const int nres = 1;

	lua_getfield(L, LUA_GLOBALSINDEX, "timeout_callback");
	lua_pushstring(L, "ERROR: Timeout");
	lua_call(L, 1, nres);
	return nres;
}

static void test_introspection_in_handler(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = assert_test_init(chunk_introspection_in_handler);
	lua_State *L1 = lua_newthread(L);

	assert_coroutine_init(L1, &default_timeout,
			      cb_introspection_in_handler);
	assert_coroutine_timeout(L1, 1);

	assert_test_cleanup(L);
}

/*
 * CASE: There is no possibility to start a timed coroutine from Lua.
 */

/*
 * coroutine_resumer is accessed from C in attempt to make Lua resume a
 * coroutine with a timeout set (the attempt is expected to fail).
 */
static const char *chunk_no_timed_resume_from_lua =
	"function coroutine_resumer(co)            \n"
	"        coroutine.resume(co)              \n"
	"end                                       \n"
	"function coroutine_start(n)               \n"
	"end                                       \n";

static void test_no_timed_resume_from_lua(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = assert_test_init(chunk_no_timed_resume_from_lua);
	lua_State *L1 = lua_newthread(L);

	assert_coroutine_init(L1, &default_timeout, NULL);

	lua_getfield(L, LUA_GLOBALSINDEX, "coroutine_resumer");
	lua_pushvalue(L, -2);

	assert_int_equal(lua_pcall(L, 1, 0, 0), LUA_ERRRUN);
	test_substring(L, 2, "resume a coroutine with timeout");

	assert_test_cleanup(L);
}

/*
 * CASE: Timeouts are ignored unless the Lua/C code was invoked with lua_resume
 * (i.e. with either lua_call or lua_pcall or lua_cpcall).
 */

static const char *chunk_timeout_ignored = "nruns = 0                   \n"
					   "function payload()          \n"
					   "        aux_sleep_lua(1)    \n"
					   "        nruns = nruns + 1   \n"
					   "end                         \n";

static void test_timeout_ignored(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = assert_test_init(chunk_timeout_ignored);
	lua_State *L1 = lua_newthread(L);

	lua_register(L1, "aux_sleep_lua", aux_sleep_lua);

	assert_true(luaE_intresolvable(&default_timeout));
	assert_int_equal(luaE_settimeout(L1, &default_timeout, 0),
			 LUAE_TIMEOUT_SUCCESS);

	lua_getglobal(L1, "payload");
	lua_call(L1, 0, 0); /* increments nruns */
	assert_stack_size(L1, 0);

	lua_getglobal(L1, "payload");
	assert_int_equal(lua_pcall(L1, 0, 0, 0), 0); /* increments nruns */
	assert_stack_size(L1, 0);

	assert_int_equal(lua_cpcall(L1, aux_sleep_lua, NULL), 0);
	assert_stack_size(L1, 0);

	lua_getglobal(L1, "payload");
	assert_int_equal(lua_resume(L1, 0), LUAE_TIMEOUT);
	assert_stack_size(L1, 0);

	lua_getglobal(L1, "nruns");
	assert_int_equal((int)lua_tointeger(L1, -1), 2);
	lua_pop(L1, 1);

	assert_stack_size(L1, 0);
	assert_test_cleanup(L);
}

/****************************** RUN ALL CASES ******************************/

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_no_timeout_for_main_thread),
		cmocka_unit_test(test_no_timeout_without_ticks),
		cmocka_unit_test(test_no_timeout_overriding),
		cmocka_unit_test(test_no_timeout_badtime),
		cmocka_unit_test(test_timeout_trivial_loop),
		cmocka_unit_test(test_timeout_trivial_call_ret),
		cmocka_unit_test(test_timeout_asap),
		cmocka_unit_test(test_timeout_asap_c),
		cmocka_unit_test(test_timeout_asap_restart),
		cmocka_unit_test(test_timeout_tricky_stack),
		cmocka_unit_test(test_timeout_with_yield),
		cmocka_unit_test(test_timeout_instant_yield),
		cmocka_unit_test(test_timeout_with_callback),
		cmocka_unit_test(test_timeout_with_callback_to_lua),
		cmocka_unit_test(test_timeout_callback_throws),
		cmocka_unit_test(test_timeout_callback_has_bad_op),
		cmocka_unit_test(test_timeout_restart),
		cmocka_unit_test(test_timeout_long_hook),
		cmocka_unit_test(test_introspection_in_handler),
		cmocka_unit_test(test_no_timed_resume_from_lua),
		cmocka_unit_test(test_timeout_ignored),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
