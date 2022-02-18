/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common_lua.h"

#include "lj_obj.h"
#include "lj_tab.h"

static size_t array_part_size(lua_State *L, unsigned int narg)
{
	const GCtab *t = tabV(L->base + narg);
	size_t size = 0;
	size_t i;

	for (i = 0; i < t->asize; i++)
		if (!tvisnil(arrayslot(t, i)))
			size++;

	return size;
}

static void test_store_num_key(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();

	/* local t = { ... } */
	lua_createtable(L, /* narr = */ 2, /* nrec = */ 4);

	/* local mt = { ... } */
	lua_newtable(L);

	/* setmetatable(t, mt) */
	lua_setmetatable(L, -2);

	/* t[1] = true */
	lua_pushinteger(L, (lua_Integer)1);
	lua_pushboolean(L, 1);
	lua_settable(L, -3);

	/* Just to be sure that the bunch of lua_* calls above is right: */
	assert_stack_size(L, 1);

	/* Previous store must go to the array part. */
	assert_int_equal(array_part_size(L, 0), 1);

	lua_close(L);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_store_num_key),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
