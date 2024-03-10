/*
 * Tests for traversal of coroutine stacks during garbage collection.
 * The main idea for the tests is to use specially crafted Lua chunks which:
 *  * Invoke metamethods creating corresponding continuation frames.
 *  * Invoke garbage collector at the time continuation frames are either
 *    active or inactive, but are not yet reused for any live value.
 * Besides, memory layout is adjusted in such a manner that PC in the
 * continuation frame "resembles" a tag of some object. Thus, if continuation
 * frames are not nil'ed on metamethod exit *or* if stack traversal is done
 * without respect to the frame structure on the stack, the tests will crash
 * because garbage collector will try to interpret continutation frame as some
 * value slot.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <stdlib.h>
#include <sys/mman.h>

#include "lj_bc.h"
#include "lj_obj.h"

#include "test_common_lua.h"

/*
 * Allocate & free custom memory arena around a "convenient" address
 */

#define NUM_CUSTOM_PAGES (0xf0)
#define CUSTOM_ARENA_SIZE ((size_t)(NUM_CUSTOM_PAGES * UJ_PAGESIZE))

static void *hint2mmap_hint(const uintptr_t hint)
{
	return (void *)((hint & ~(UJ_PAGESIZE - 1)) - 0xf * UJ_PAGESIZE);
}

void *custom_arena_alloc(const uintptr_t hint)
{
	void *mmap_hint;
	void *arena;

	assert_true(0 != hint);

	mmap_hint = hint2mmap_hint(hint);
	arena = mmap(mmap_hint, CUSTOM_ARENA_SIZE, PROT_READ | PROT_WRITE,
		     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	assert_true(NULL != arena && arena == mmap_hint);
	assert_true((void *)hint > arena);
	assert_true((uint8_t *)hint <
		    (uint8_t *)arena + (ptrdiff_t)CUSTOM_ARENA_SIZE);

	return arena;
}

int custom_arena_free(void *arena)
{
	return munmap(arena, CUSTOM_ARENA_SIZE);
}

/*
 * Find a proper placement for an object inside the custom arena, copy
 * object and adjust the GC chain.
 */

void *custom_proto_alloc(uintptr_t addr, const GCproto *pt, const BCOp op)
{
	const BCIns *bc = proto_bc(pt);
	BCPos i;

	for (i = 0; i < pt->sizebc; i++)
		if (bc_op(bc[i]) == op) {
			void *target = (void *)(addr - sizeof(GCproto) -
						sizeof(BCIns) * (i + 1));
			return (GCproto *)target;
		}

	return NULL;
}

void custom_proto_copy(GCproto *dst, const GCproto *src)
{
	memcpy(dst, src, src->sizept);
}

/* Replace the first proto defined below some hard-coded line. */
static int proto_is_replaceable(const GCproto *pt)
{
	return pt->firstline > 25;
}

/* Find a proto that will be replaced with a custom re-located copy. */
static GCproto *proto_find_replaceable(global_State *g)
{
	GCobj *o, **iter = &g->gc.root;

	while (NULL != (o = *iter)) {
		if (o->gch.gct == ~LJ_TPROTO) {
			GCproto *pt = gco2pt(o);

			if (proto_is_replaceable(pt))
				return pt;
		}
		iter = &o->gch.nextgc;
	}
	return NULL;
}

/*
 * Patch the table of constants of the proto pt_parent replacing
 * pt_old with pt_new. Returns number of adjustments made.
 */
static int patch_parent_proto_kgc(GCproto *pt_parent, GCproto *pt_old,
				  GCproto *pt_new)
{
	ptrdiff_t idx;
	int num_adjustments = 0;

	assert((pt_parent->flags & PROTO_CHILD));

	for (idx = -1; idx >= -(ptrdiff_t)pt_parent->sizekgc; idx--) {
		GCobj *gc = proto_kgc(pt_parent, idx);

		if (gc->gch.gct != ~LJ_TPROTO)
			continue;
		if (gco2pt(gc) == pt_old) {
			((GCobj **)(pt_parent->k))[idx] = (GCobj *)pt_new;
			num_adjustments++;
		}
	}

	return num_adjustments;
}

/*
 * Exclude gco_old from the GC chain replacing all its occurrences with gco_new.
 * Returns number of adjustments made.
 */
static int patch_gco_chain(global_State *g, GCobj *gco_old, GCobj *gco_new)
{
	GCobj *o, **iter = &g->gc.root;
	int num_adjustments = 0;

	while (NULL != (o = *iter)) {
		if (o->gch.nextgc == gco_old) {
			o->gch.nextgc = gco_new;
			num_adjustments++;
		}
		iter = &o->gch.nextgc;
	}

	return num_adjustments;
}

/*
 * Main test routine
 */

static void run(const char *chunk_fname)
{
	const uintptr_t addr = (uintptr_t)(0x7f3000000000 | LJ_TPROTO);
	void *arena = custom_arena_alloc(addr);
	lua_State *L = lua_open();
	global_State *g = G(L);
	GCproto *pt_main;
	GCproto *pt_orig;
	GCproto *pt_custom;
	int status;

	assert_non_null(arena);
	assert_non_null(L);
	assert_non_null(g);

	/* Initialize Lua VM and load the chunk */
	luaL_openlibs(L);
	assert_int_equal(luaL_loadfile(L, chunk_fname), 0);
	assert_true(tvisfunc(L->top - 1) && isluafunc(funcV(L->top - 1)));

	/* Find an appropriate proto and copy it to a convenient memory area */
	pt_main = funcproto(funcV(L->top - 1));
	assert_non_null(pt_main);
	pt_orig = proto_find_replaceable(g);
	assert_non_null(pt_orig);
	pt_custom = custom_proto_alloc(addr, pt_orig, BC_MUL);
	assert_non_null(pt_custom);
	assert((void *)pt_custom >= arena);
	assert((uint8_t *)pt_custom + (ptrdiff_t)pt_orig->sizept <=
	       (uint8_t *)arena + (ptrdiff_t)CUSTOM_ARENA_SIZE);
	custom_proto_copy(pt_custom, pt_orig);

	/* Patch the VM to start using our custom-located proto */
	status = patch_parent_proto_kgc(pt_main, pt_orig, pt_custom);
	assert_int_equal(status, 1);
	status = patch_gco_chain(g, (GCobj *)pt_orig, (GCobj *)pt_custom);
	assert_int_equal(status, 1);

	/* Run Lua */
	assert_int_equal(lua_pcall(L, 0, LUA_MULTRET, 0), 0);

	/* Put the original proto back to VM. */
	status = patch_gco_chain(g, (GCobj *)pt_custom, (GCobj *)pt_orig);
	assert_int_equal(status, 1);
	status = patch_parent_proto_kgc(pt_main, pt_custom, pt_orig);
	assert_int_equal(status, 1);

	lua_close(L);
	assert_int_equal(custom_arena_free(arena), 0);
}

static void test_gc_traverse_stack_before_mm_exit(void **state)
{
	UNUSED_STATE(state);

	run("chunks/test_gc_traverse_stack/gc_before_mm_exit.lua");
}

static void test_gc_traverse_stack_after_mm_exit(void **state)
{
	UNUSED_STATE(state);

	run("chunks/test_gc_traverse_stack/gc_after_mm_exit.lua");
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_gc_traverse_stack_before_mm_exit),
		cmocka_unit_test(test_gc_traverse_stack_after_mm_exit),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
