/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <stdlib.h>
#include "test_common_lua.h"

static unsigned int mark = 0xdeadbeef;

static void *allocf(void *ud, void *ptr, size_t osize, size_t nsize)
{
	UNUSED(osize);

	assert_int_equal(*(unsigned int *)ud, 0xdeadbeef);

	if (0 == nsize) {
		free(ptr);
		return NULL;
	}

	return realloc(ptr, nsize);
}

static void test_lua_alloc_newstate(void **state)
{
	UNUSED_STATE(state);

	void *newmark;
	lua_State *L = lua_newstate(allocf, &mark);

	assert_non_null(L);
	assert_ptr_equal(allocf, lua_getallocf(L, &newmark));
	assert_int_equal(*(unsigned int *)newmark, mark);

	luaL_openlibs(L);
	assert_int_equal(luaL_dostring(L, "print('Hello, world')"), 0);

	lua_close(L);
}

static void test_lua_alloc_inspect(void **state)
{
	UNUSED_STATE(state);

	lua_State *L = test_lua_open();

	assert_non_null(lua_getallocf(L, NULL));
	lua_close(L);
}

struct alloc_counters {
	size_t nmalloc;
	size_t nrealloc;
	size_t nfree;
};

static struct alloc_counters acnt = {0};

static lua_Alloc allocf_orig;

static void *allocf_probe(void *ud, void *ptr, size_t osize, size_t nsize)
{
	if (0 == nsize)
		acnt.nfree++;
	else if (NULL == ptr)
		acnt.nmalloc++;
	else
		acnt.nrealloc++;

	return allocf_orig(ud, ptr, osize, nsize);
}

static void test_lua_alloc_probe(void **state)
{
	UNUSED_STATE(state);

	lua_State *L;
	void *state_orig;

	L = test_lua_open();
	allocf_orig = lua_getallocf(L, &state_orig);
	lua_setallocf(L, allocf_probe, state_orig);

	lua_createtable(L, 0, 16);
	assert_stack_size(L, 1);

	lua_pop(L, 1);
	lua_gc(L, LUA_GCCOLLECT, 0);
	assert_stack_size(L, 0);

	/* Let's make sure that allocations are counted: */
	assert_true(acnt.nmalloc >= 1);
	assert_int_equal(acnt.nmalloc, acnt.nfree);
	assert_int_equal(acnt.nrealloc, 0);

	/*
	 * When you probe the original allocator, you can obviously do
	 * whatever you like with your environment. And it is not necessary
	 * to lua_setallocf to restore the original allocator.
	 */
	luaL_openlibs(L);

	lua_close(L);
	/*
	 * Please do *not* assume that acnt.nmalloc == acnt.nfree here:
	 * Some allocations were made during creation of L, with the default
	 * allocator (not allocf_probe), so stats won't match. On the bright
	 * side, we assert memory consumption inside the code base.
	 */
}

/*
 * NB! It is a very bad idea to try to substitute allocator "on the fly", and
 * it certainly will not work for real-world tasks. However, here is the toy
 * code that demonstrates this "feature". Caveats:
 *  * Do not try luaL_openlibs (really, don't)
 *  * Do not try to access the standard library and _G in the chunk
 */
static void test_lua_alloc_substitute(void **state)
{
	UNUSED_STATE(state);

	lua_State *L;
	lua_Alloc allocf_orig;
	void *state_orig;
	const char *chunk = "local function sum(a, b)    \n"
			    "        return a + b        \n"
			    "end                         \n"
			    "local x = 2                 \n"
			    "local y = 3                 \n"
			    "return sum(x, y)            \n";

	L = test_lua_open();
	allocf_orig = lua_getallocf(L, &state_orig);
	lua_setallocf(L, allocf, &mark);

	lua_createtable(L, 0, 16);
	assert_stack_size(L, 1);

	lua_pop(L, 1);
	lua_gc(L, LUA_GCCOLLECT, 0);
	assert_stack_size(L, 0);

	assert_int_equal(luaL_dostring(L, chunk), 0);
	assert_stack_size(L, 1);
	test_integer(L, -1, (lua_Integer)5);

	/*
	 * A full GC cycle must be enforced before returning
	 * to the original allocator:
	 */
	lua_pop(L, 1);
	lua_gc(L, LUA_GCCOLLECT, 0);
	assert_stack_size(L, 0);

	lua_setallocf(L, allocf_orig, state_orig);
	lua_close(L);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_lua_alloc_newstate),
		cmocka_unit_test(test_lua_alloc_inspect),
		cmocka_unit_test(test_lua_alloc_probe),
		cmocka_unit_test(test_lua_alloc_substitute),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
