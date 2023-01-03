/*
 * Tests for external events running by the paltform in coroutine's context.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <signal.h>
#include <sys/time.h>

#include "test_common_lua.h"

static const struct timeval default_timeout = {
	.tv_sec = 0,
	.tv_usec = 100000 /* 0.1 sec */
};

static const char *tracing_during_timeout_chunk_fname =
	"chunks/test_ext_events/tracing_during_timeout.lua";

static int cb_timeout(lua_State *L)
{
	lua_getglobal(L, "timeout_handler");
	lua_pcall(L, 0, 0, 0);

	return 0;
}

static void test_tracing_during_timeout(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();
	lua_State *L1;

	luaL_openlibs(L);
	assert_int_equal(luaE_intinit(SIGALRM), LUAE_INT_SUCCESS);
	assert_int_equal(luaL_dofile(L, tracing_during_timeout_chunk_fname), 0);

	L1 = lua_newthread(L);

	assert_int_equal(luaE_settimeout(L1, &default_timeout, 0),
			 LUAE_TIMEOUT_SUCCESS);
	assert_ptr_equal(luaE_settimeoutf(L1, cb_timeout), NULL);

	lua_getglobal(L1, "coroutine_payload");
	assert_int_equal(lua_resume(L1, 0), LUAE_TIMEOUT);

	lua_getglobal(L1, "timeout_handler_called");
	assert_int_equal((int)lua_tonumber(L1, -1), 1);

	assert_int_equal(luaE_intterm(), LUAE_INT_SUCCESS);
	lua_close(L);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_tracing_during_timeout)};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
