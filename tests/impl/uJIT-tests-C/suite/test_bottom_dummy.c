/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <unistd.h>

#include "test_common.h"
#include "lauxlib.h"
#include "lj_obj.h"
#include "lj_frame.h"
#include "dump/uj_dump_iface.h"

static void test_empty_stack(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = lua_open();
	FILE *dump = tmpfile();

	assert_non_null(L);
	assert_non_null(dump);
	assert_ptr_equal(frame_gc(L->stack), obj2gco(L));

	GCfunc *fn = (GCfunc *)obj2gco(L);

	assert_true(iscfunc(fn));
	assert_ptr_equal(L->stack, frame_prev(L->stack));
	assert_true(frame_isbottom(L->stack));
	assert_true(frame_isdummy(L, L->stack));

	uj_dump_stack(dump, L);

	assert_false(fclose(dump));

	lua_close(L);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_empty_stack),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
