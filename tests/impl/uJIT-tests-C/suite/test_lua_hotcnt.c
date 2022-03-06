/*
 * A set of tests for new hotcounting mechanism.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common_lua.h"

#include "uj_dispatch.h"
#include "uj_hotcnt.h"

static const char *chunk_all = "chunks/bc_hotcnt/all.lua";

/* Check that current instruction is HOTCNT */
static int test_is_hotcnt(BCIns ins)
{
	return bc_op(ins) == BC_HOTCNT;
}

/* Get proto of a function from the top of stack */
static GCproto *test_func_proto(lua_State *L)
{
	TValue *tv = L->top - 1;
	GCfunc *fn = funcV(tv);
	GCproto *pt = funcproto(fn);
	return pt;
}

/* Adjusting counter for prologues */
static void test_check_counter(const BCIns *ins, uint16_t hotloop)
{
	const uint16_t count = *((uint16_t *)((char *)ins + COUNTER_OFFSET));

	/* To undestand constants, please see comments in hotcnt_adjust() */
	if (bc_op(ins[-PROLOGUE_OFFSET]) == BC_FUNCF)
		assert_int_equal(2 * hotloop - 1, count);
	else if (bc_op(ins[ITERL_OFFSET]) == BC_ITERL)
		assert_int_equal(hotloop - 1, count);
	else
		assert_int_equal(hotloop, count);
}

static void test_check_counters(const GCproto *pt, uint16_t hotloop)
{
	const BCIns *bc = proto_bc(pt);
	size_t i = 0;

	for (; i < pt->sizebc; i++) {
		if (test_is_hotcnt(bc[i]))
			test_check_counter(&bc[i], hotloop);
	}
}

/* When file is loaded, there must be default counters */
static void test_hotcnt_update_counters(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();
	jit_State *J = L2J(L);
	GCproto *pt;
	uint16_t hotloop;

	luaL_openlibs(L);
	hotloop = (uint16_t)J->param[JIT_P_hotloop];

	assert_int_equal(luaL_dofile(L, chunk_all), 0);
	assert_stack_size(L, 0);

	lua_getglobal(L, "all_cases");
	assert_stack_size(L, 1);
	pt = test_func_proto(L);

	/* Check for default counters */
	test_check_counters(pt, hotloop);

	/* Check for updated counters */
	hotloop = 456;
	lua_getglobal(L, "update_counters");
	lua_pushinteger(L, hotloop);
	assert_stack_size(L, 3);
	lua_call(L, 1, 0);
	assert_stack_size(L, 1);
	lua_getglobal(L, "all_cases");
	pt = test_func_proto(L);
	test_check_counters(pt, hotloop);

	lua_close(L);
}

static void test_hotcnt_dump_load(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();
	jit_State *J = L2J(L);
	GCproto *pt;
	uint16_t hotloop;
	const char *bcdump;
	GCstr *str;
	size_t i;
	/* 6 HOTCNT instructions in all_cases() function */
	size_t instr_cnt = 6;

	luaL_openlibs(L);
	hotloop = (uint16_t)J->param[JIT_P_hotloop];

	assert_int_equal(luaL_dofile(L, chunk_all), 0);
	assert_stack_size(L, 0);

	lua_getglobal(L, "dumped_all_cases");
	assert_stack_size(L, 1);

	bcdump = strVdata(L->top - 1);
	str = strV(L->top - 1);

	/*
	 * Check for zeroes backwards, because some bytes in prologue of dump
	 * may coincide with HOTCNT opcode.
	 */
	for (i = str->len - 1; i != 0; i--) {
		if (instr_cnt == 0)
			break;
		if (bcdump[i] == BC_HOTCNT) {
			/* Flags, currently unused */
			assert_int_equal(bcdump[i + 1], 0);
			/* Counter high part */
			assert_int_equal(bcdump[i + 2], 0);
			/* Counter low part */
			assert_int_equal(bcdump[i + 3], 0);
			/* Decrease HOTCNT instruction counter */
			instr_cnt -= 1;
		}
	}

	/* Check that original counters doesn't changed */
	lua_getglobal(L, "all_cases");
	assert_stack_size(L, 2);
	pt = test_func_proto(L);
	test_check_counters(pt, hotloop);

	/* Check that loaded function has default counters */
	lua_getglobal(L, "loaded_all_cases");
	assert_stack_size(L, 3);
	pt = test_func_proto(L);
	test_check_counters(pt, hotloop);

	lua_close(L);
}

/****************************** RUN ALL CASES ******************************/

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_hotcnt_update_counters),
		cmocka_unit_test(test_hotcnt_dump_load),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
