/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common_lua.h"

static void test_empty_iteration(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();
	uint64_t iter;
	int n_keys = 0;

	lua_createtable(L, 0, 16);
	assert_stack_size(L, 1);

	iter = LUAE_ITER_BEGIN;
	while ((iter = luaE_iterate(L, -1, iter)) != LUAE_ITER_END) {
		n_keys++;
		lua_pop(L, 2);
	}
	assert_stack_size(L, 1);
	assert_int_equal(n_keys, 0);

	lua_close(L);
}

static void test_simple_array(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();
	uint64_t iter;
	int n_keys = 0;

	/* t = {"foo"} */

	lua_createtable(L, 16, 0);
	assert_stack_size(L, 1);

	lua_pushinteger(L, (lua_Integer)1);
	lua_pushstring(L, "foo");
	lua_settable(L, -3);
	assert_stack_size(L, 1);

	iter = LUAE_ITER_BEGIN;
	while ((iter = luaE_iterate(L, -1, iter)) != LUAE_ITER_END) {
		n_keys++;
		assert_stack_size(L, 3);
		test_number(L, -2, (lua_Number)1);
		test_string(L, -1, "foo");
		lua_pop(L, 2);
	}
	assert_stack_size(L, 1);
	assert_int_equal(n_keys, 1);

	lua_close(L);
}

static void test_holey_array(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();
	uint64_t iter;
	uint8_t used_key = 0;
	uint8_t used_key_exp = 0;
	int i;
	int n_keys = 0;

	/* t = {[2] = 12, [4] = 14, [8] = 18} */

	lua_createtable(L, 16, 0);
	assert_stack_size(L, 1);

	for (i = 1; i <= 3; i++) {
		lua_Integer key = (lua_Integer)(1 << i);

		used_key_exp |= (uint8_t)key;
		lua_pushinteger(L, key);
		lua_pushinteger(L, (lua_Integer)10 + key);
		lua_settable(L, -3);
		assert_stack_size(L, 1);
	}

	/* Just to demonstrate for(...) syntax with luaE_iterate */
	for (iter = luaE_iterate(L, -1, LUAE_ITER_BEGIN); iter != LUAE_ITER_END;
	     iter = luaE_iterate(L, -1, iter)) {
		lua_Number key;

		n_keys++;
		assert_stack_size(L, 3);
		assert_int_equal(lua_isnumber(L, -2), 1);
		assert_int_equal(lua_isnumber(L, -1), 1);
		key = lua_tonumber(L, -2);
		used_key |= (uint8_t)key;
		assert_int_equal((int)lua_tonumber(L, -1), 10 + (int)key);
		lua_pop(L, 2);
	}
	assert_stack_size(L, 1);
	assert_int_equal(n_keys, 3);
	assert_int_equal(used_key, used_key_exp);

	lua_close(L);
}

static void test_simple_hash(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();
	uint64_t iter;
	int n_keys = 0;

	/* t = {foo = "bar"} */

	lua_createtable(L, 0, 16);
	assert_stack_size(L, 1);

	lua_pushstring(L, "bar");
	lua_setfield(L, -2, "foo");
	assert_stack_size(L, 1);

	iter = LUAE_ITER_BEGIN;
	while ((iter = luaE_iterate(L, -1, iter)) != LUAE_ITER_END) {
		n_keys++;
		assert_stack_size(L, 3);
		test_string(L, -2, "foo");
		test_string(L, -1, "bar");
		lua_pop(L, 2);
	}
	assert_stack_size(L, 1);
	assert_int_equal(n_keys, 1);

	lua_close(L);
}

static void test_simple_mixed(void **state)
{
	UNUSED_STATE(state);

	const uint32_t small_key = ((uint32_t)1) << 1;
	const uint32_t large_key = ((uint32_t)1) << 16;

	lua_State *L = test_lua_open();
	uint64_t iter;
	uint64_t used_key = 0;
	int n_keys = 0;

	/* t = {foo = "bar", [small_key] = "bar", [large_key] = "bar"} */

	lua_createtable(L, 16, 16);
	assert_stack_size(L, 1);

	lua_pushstring(L, "bar");
	lua_setfield(L, -2, "foo");
	assert_stack_size(L, 1);

	lua_pushinteger(L, (lua_Integer)small_key);
	lua_pushstring(L, "bar");
	lua_settable(L, -3);
	assert_stack_size(L, 1);

	lua_pushinteger(L, (lua_Integer)large_key);
	lua_pushstring(L, "bar");
	lua_settable(L, -3);
	assert_stack_size(L, 1);

	iter = LUAE_ITER_BEGIN;
	while ((iter = luaE_iterate(L, -1, iter)) != LUAE_ITER_END) {
		n_keys++;
		assert_stack_size(L, 3);

		if (!lua_isnumber(L, -2))
			test_string(L, -2, "foo");
		else
			used_key |= (uint32_t)lua_tonumber(L, -2);

		test_string(L, -1, "bar"); /* but the same value every time */
		lua_pop(L, 2);
	}
	assert_stack_size(L, 1);
	assert_int_equal(n_keys, 3);
	assert_true(used_key == (small_key | large_key));

	lua_close(L);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_empty_iteration),
		cmocka_unit_test(test_simple_array),
		cmocka_unit_test(test_holey_array),
		cmocka_unit_test(test_simple_hash),
		cmocka_unit_test(test_simple_mixed),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
