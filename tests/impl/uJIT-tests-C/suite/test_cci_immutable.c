/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common.h"
#include "jit/lj_ircall.h"

/* Tests that all CCallInfo with CCI_IMMUTABLE flag are Stores */

static void test_cci_immutable(void **state)
{
	UNUSED_STATE(state);

	IRCallID id;

	for (id = 0; id < IRCALL__MAX; ++id) {
		const CCallInfo *cci = lj_ir_callinfo + id;

		if (cci->flags & CCI_IMMUTABLE)
			assert_true(CCI_OP(cci) == IR_CALLS);
	}
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_cci_immutable)};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
