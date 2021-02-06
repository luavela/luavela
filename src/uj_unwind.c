/*
 * Implementation of uJIT's internal unwinder.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "lj_obj.h"
#include "uj_state.h"
#include "lj_frame.h"
#include "uj_cframe.h"
#include "uj_upval.h"
#include "profile/uj_iprof_iface.h"

#include "lextlib.h"

struct unwind_context {
	void *cframe; /* Current C frame */
	TValue *frame; /* Current Lua frame */
	size_t nres; /* # of top slots to keep during unwinding */
	int ex; /* Exception type */
	uint8_t frame_type; /* Current Lua frame type */
};

/*
 * For frames created by vm_cpcall: Returns true if current C frame is the one
 * without Lua frame (convention: nres must be negative) and guest unwinding
 * has reached it.
 */
static int unwind_inside_cpcall_frame(const lua_State *L, const void *cframe,
				      const TValue *frame)
{
	const int64_t nres = uj_cframe_nres(uj_cframe_raw(cframe));
	return nres < 0 && frame < uj_state_stack_restore(L, -nres);
}

/****************************** SEARCH PHASE ******************************/

/*
 * Per-frame type callbacks called during search phase.
 */

typedef void *(*search_callback)(int ex, const void *cframe);

/* Default callback: Current C frame does not catch the exception. */
LJ_NOINLINE static void *unwind_search_default(int ex, const void *cframe)
{
	UNUSED(ex);
	UNUSED(cframe);
	return NULL;
}

/*
 * FRAME_CP:
 *  * LUAE_TIMEOUT: Catches if current C frame was created by vm_resume,
 *    does not catch otherwise.
 *  * Other exceptions: Always catches.
 */
LJ_NOINLINE static void *unwind_search_frame_cp(int ex, const void *cframe)
{
	if (ex != LUAE_TIMEOUT)
		return (void *)cframe;

	return uj_cframe_canyield(cframe) ? (void *)cframe : NULL;
}

/*
 * FRAME_PCALL, FRAME_PCALLH:
 *  * LUAE_TIMEOUT: Never catches.
 *  * Other exceptions: Always catches.
 */
LJ_NOINLINE static void *unwind_search_frame_pcall(int ex, const void *cframe)
{
	return ex != LUAE_TIMEOUT ? uj_cframe_unwind_mark_ff(cframe) : NULL;
}

/*
 * vm_cpcall:
 *  * LUAE_TIMEOUT: Never catches.
 *  * Other exceptions: Always catches.
 */
LJ_NOINLINE static void *unwind_search_frame_cpcall(int ex, const void *cframe)
{
	return ex != LUAE_TIMEOUT ? (void *)cframe : NULL;
}

/* ORDER FRAME_ */
static const search_callback search_callbacks[] = {
	/* FRAME_LUA:    */ unwind_search_default,
	/* FRAME_C:      */ unwind_search_default,
	/* FRAME_CONT:   */ unwind_search_default,
	/* FRAME_VARG:   */ unwind_search_default,
	/* FRAME_LUAP:   */ unwind_search_default,
	/* FRAME_CP:     */ unwind_search_frame_cp,
	/* FRAME_PCALL:  */ unwind_search_frame_pcall,
	/* FRAME_PCALLH: */ unwind_search_frame_pcall};

/****************************** CLEANUP PHASE ******************************/

/*
 * Various aux routines for cleanup.
 */

/* Unwind Lua stack and move nres slots to the new top. */
LJ_NOINLINE static void unwind_cleanup_stack(lua_State *L, TValue *top,
					     size_t nres)
{
	uj_upval_close(L, top);
	if (top < L->top - nres) {
		size_t i;
		for (i = 0; i < nres; i++)
			copyTV(L, top + i, L->top + i - nres);
		L->top = top + nres;
	}
	uj_state_stack_relimit(L);
}

/* Perform a cleanup if unwinding fails and exit abnormally. */
static void unwind_cleanup_panic(lua_State *L, size_t nres)
{
	L->cframe = NULL;
	L->base = L->stack + 1;
	unwind_cleanup_stack(L, L->base, nres);
	if (G(L)->panic)
		G(L)->panic(L);
	exit(EXIT_FAILURE);
}

/* Perform a cleanup when unwinding crosses C frame boundary. */
static void unwind_cleanup_across_c(lua_State *L,
				    const struct unwind_context *ctx,
				    int is_resume)
{
	lua_assert(!frame_islua(ctx->frame));

	if (!is_resume)
		L->cframe = uj_cframe_prev(ctx->cframe);

	L->base = frame_prev(ctx->frame) + 1;
	unwind_cleanup_stack(L, ctx->frame, ctx->nres);
}

/* Halt a coroutine. */
static void unwind_halt_coroutine(lua_State *L, int ex)
{
	hook_leave(G(L)); /* Assumes nobody uses coroutines inside hooks. */
	L->cframe = NULL;
	L->status = (uint8_t)ex;
}

/*
 * Per-frame type callbacks called during cleanup phase.
 */

typedef void (*cleanup_callback)(lua_State *L,
				 const struct unwind_context *ctx);

/* Default callback: Perform no cleanup. */
LJ_NOINLINE static void unwind_cleanup_default(lua_State *L,
					       const struct unwind_context *ctx)
{
	UNUSED(ctx);

#ifdef UJIT_IPROF_ENABLED
	uj_iprof_unwind(L);
#else
	UNUSED(L);
#endif
}

/* FRAME_C */
LJ_NOINLINE static void unwind_cleanup_frame_c(lua_State *L,
					       const struct unwind_context *ctx)
{
	unwind_cleanup_across_c(L, ctx, 0);

#ifdef UJIT_IPROF_ENABLED
	uj_iprof_unwind(L);
#endif
}

/* FRAME_CP (including vm_resume) */
LJ_NOINLINE static void
unwind_cleanup_frame_cp(lua_State *L, const struct unwind_context *ctx)
{
	void *cframe = ctx->cframe;
	int is_resume = uj_cframe_canyield(cframe);

	if (is_resume)
		unwind_halt_coroutine(L, ctx->ex);

	if (ctx->ex != LUAE_TIMEOUT && is_resume)
		return; /* No cleanup: Coroutine's stack must be kept intact. */

	if (ctx->ex == LUAE_TIMEOUT && !is_resume)
		return; /* No cleanup: Not our target frame. */

	lua_assert((ctx->ex != LUAE_TIMEOUT && !is_resume) ||
		   (ctx->ex == LUAE_TIMEOUT && is_resume));

	unwind_cleanup_across_c(L, ctx, is_resume);

#ifdef UJIT_IPROF_ENABLED
	uj_iprof_unwind(L);
#endif
}

/* FRAME_PCALL, FRAME_CALLH */
LJ_NOINLINE static void
unwind_cleanup_frame_pcall(lua_State *L, const struct unwind_context *ctx)
{
	lua_assert(!frame_islua(ctx->frame));

	if (LUAE_TIMEOUT == ctx->ex)
		return; /* No cleanup: Not our target frame. */

	if (LUA_YIELD == ctx->ex)
		return; /* No cleanup: Coroutine yields normally. */

	if (FRAME_PCALL == ctx->frame_type)
		hook_leave(G(L));

	lua_assert(ctx->ex != LUAE_TIMEOUT && ctx->ex != LUA_YIELD);

	L->cframe = ctx->cframe;
	L->base = frame_prev(ctx->frame) + 1;
	unwind_cleanup_stack(L, L->base, ctx->nres);

#ifdef UJIT_IPROF_ENABLED
	uj_iprof_unwind(L);
#endif
}

/* vm_cpcall */
LJ_NOINLINE static void
unwind_cleanup_frame_cpcall(lua_State *L, const struct unwind_context *ctx)
{
	int64_t nres;
	TValue *top;

	lua_assert(unwind_inside_cpcall_frame(L, ctx->cframe, ctx->frame));

	nres = uj_cframe_nres(uj_cframe_raw(ctx->cframe));
	top = uj_state_stack_restore(L, -nres);

	L->cframe = uj_cframe_prev(ctx->cframe);
	L->base = ctx->frame + 1;
	unwind_cleanup_stack(L, top, ctx->nres);

#ifdef UJIT_IPROF_ENABLED
	uj_iprof_unwind(L);
#endif
}

/* ORDER FRAME_ */
static const cleanup_callback cleanup_callbacks[] = {
	/* FRAME_LUA:    */ unwind_cleanup_default,
	/* FRAME_C:      */ unwind_cleanup_frame_c,
	/* FRAME_CONT:   */ unwind_cleanup_default,
	/* FRAME_VARG:   */ unwind_cleanup_default,
	/* FRAME_LUAP:   */ unwind_cleanup_default,
	/* FRAME_CP:     */ unwind_cleanup_frame_cp,
	/* FRAME_PCALL:  */ unwind_cleanup_frame_pcall,
	/* FRAME_PCALLH: */ unwind_cleanup_frame_pcall};

/****************************** STACK TRAVERSAL ******************************/

/*
 * Traverse host and guest stacks simultaneously (no further than ext_cframe
 * points to). On each traversal step, attempt to find a catch frame for the
 * exception ex. If cleanup flag is set, perform appropriate stack cleanup.
 * This routine is invoked each time the external unwinder encounters a host
 * stack frame which should be handled by our DWARF2 personality routine. And
 * each time this routine is invoked, we have to repeat the traversal because
 * L->cframe and L->base track information only about the topmost host and guest
 * frames, respectively. But `ext_cframe` is different each time as the external
 * unwinder moves along the host stack.
 */
static void *unwind_traverse(lua_State *L, int ex, const void *ext_cframe,
			     int cleanup)
{
	TValue *frame = L->base - 1;
	void *cframe = L->cframe;
	const size_t nres = LUAE_TIMEOUT == ex ? L->timeout.nres : 1;

	struct unwind_context ctx = {/* Fill loop invariants */
				     .ex = ex,
				     .nres = nres};

	while (cframe) {
		void *target;
		uint8_t frame_type = frame_type_unwind(frame);

		if (cleanup) {
			ctx.cframe = cframe;
			ctx.frame = frame;
			ctx.frame_type = frame_type;
		}

		if (unwind_inside_cpcall_frame(L, cframe, frame)) {
			target = unwind_search_frame_cpcall(ex, cframe);
			if (cleanup)
				unwind_cleanup_frame_cpcall(L, &ctx);
			if (target != NULL)
				return target; /* Stop unwinding. */
		}

		/*
		 * Frame too low and we are not in the cpcall frame?
		 * Probably the stack is corrupted and "unexpectedly" unwound.
		 */
		if (frame <= L->stack)
			break;

		target = (search_callbacks[frame_type])(ex, cframe);
		if (cleanup)
			(cleanup_callbacks[frame_type])(L, &ctx);
		if (target != NULL)
			return target; /* Stop unwinding. */

		if (frame_type == FRAME_C || frame_type == FRAME_CP) {
			const void *cframe_raw = uj_cframe_raw(cframe);

			if (cframe_raw == ext_cframe)
				return NULL; /* Continue external unwinding. */

			cframe = uj_cframe_prev(cframe_raw);
		}

		frame = frame_prev(frame); /* Continue internal unwinding. */
	}

	/* No C frame. */
	if (cleanup)
		unwind_cleanup_panic(L, nres);

	return L; /* Anything non-NULL will do. */
}

/****************************** PUBLIC API ******************************/

void *uj_unwind_search(lua_State *L, int ex, const void *ext_cframe)
{
	return unwind_traverse(L, ex, ext_cframe, 0);
}

void *uj_unwind_cleanup(lua_State *L, int ex, const void *ext_cframe)
{
	return unwind_traverse(L, ex, ext_cframe, 1);
}
