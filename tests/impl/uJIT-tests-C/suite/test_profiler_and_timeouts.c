/*
 * Test simultaneous usage of profiler and timeouts.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <signal.h>
#include <sys/time.h>
#include "test_common_lua.h"

static const struct timeval default_timeout = {
	.tv_sec = 0,
	.tv_usec = 200000 /* 0.2 sec */
};

static const char *profile_timeouts_chunk_fname =
	"chunks/test_profiler_and_timeouts/profile_timeouts.lua";

/* Run a coroutine that will expire while profiling is on. */
static void run_coroutine(lua_State *L)
{
	lua_State *L1;

	assert_stack_size(L, 0);

	L1 = lua_newthread(L);
	assert_int_equal(luaE_settimeout(L1, &default_timeout, 0), 0);

	lua_getfield(L1, LUA_GLOBALSINDEX, "coroutine_start");
	lua_pushinteger(L1, (lua_Integer)10);

	assert_int_equal(lua_resume(L1, 1), LUAE_TIMEOUT);
	assert_stack_size(L1, 0);

	lua_pop(L, 1);
	assert_stack_size(L, 0);
}

static void test_profile_timeouts(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();
	size_t i;

	luaL_openlibs(L);

	/* Initialize timer interrupts and load the chunk */
	assert_int_equal(luaE_intinit(SIGPROF), LUAE_INT_SUCCESS);
	assert_int_equal(luaL_dofile(L, profile_timeouts_chunk_fname), 0);

	/* Start profiling. */
	lua_getfield(L, LUA_GLOBALSINDEX, "chunk_start");
	assert_int_equal(lua_pcall(L, 0, 0, 0), 0);

	for (i = 0; i < 15; i++)
		run_coroutine(L);

	/* Stop profiling. */
	lua_getfield(L, LUA_GLOBALSINDEX, "chunk_exit");
	assert_int_equal(lua_pcall(L, 0, 0, 0), 0);

	assert_int_equal(luaE_intterm(), LUAE_INT_SUCCESS);
	lua_close(L);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_profile_timeouts),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
