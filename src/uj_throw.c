/*
 * Implementation of throwing errors.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "lj_obj.h"
#include "uj_throw.h"
#include "uj_errmsg.h"
#include "uj_unwind_ext.h"
#include "lj_frame.h"
#include "uj_cframe.h"
#include "uj_state.h"
#include "uj_vmstate.h"
#include "jit/lj_trace.h"

#include "lextlib.h"

/* No error function (aka error handler) found + stop looking for it. */
#define ERRF_FIND_NOT_FOUND ((ptrdiff_t)(0))

/* Continue looking for an error function (aka error handler). */
#define ERRF_FIND_CONTINUE ((ptrdiff_t)(-1))

static LJ_AINLINE uint64_t throw_uexclass_make(int errcode)
{
	return UJ_UEXCLASS | (uint64_t)errcode;
}

static __thread struct _Unwind_Exception static_uex = {0};

LJ_NOINLINE void uj_throw(struct lua_State *L, int errcode)
{
	global_State *g = G(L);

	lj_trace_abort(g);
	g->jit_L = NULL;
	L->status = 0;

	/*
	 * PROFILER: We are about to destroy the stack of running coroutine,
	 * so let's forbid any attempts to access it asynchronously.
	 */
	uj_vmstate_set(&g->vmstate, UJ_VMST_INTERP);

	static_uex.exclass = throw_uexclass_make(errcode);
	static_uex.excleanup = NULL;
	_Unwind_RaiseException(&static_uex);

	/*
	 * A return from this function signals a corrupt C stack that cannot be
	 * unwound. We have no choice but to call the panic function and exit.
	 *
	 * Usually this is caused by a C function without unwind information.
	 * This should never happen on x64, but may happen if you've manually
	 * enabled LUAJIT_UNWIND_EXTERNAL and forgot to recompile *every*
	 * non-C++ file with -funwind-tables.
	 */
	if (g->panic)
		g->panic(L);

	exit(EXIT_FAILURE);
}

typedef ptrdiff_t (*errf_find_callback)(const lua_State *L, const TValue *frame,
					const void *cframe);

/* Default callback: No error function here, continue searching. */
LJ_NOINLINE static ptrdiff_t throw_errf_find_default(const lua_State *L,
						     const TValue *frame,
						     const void *cframe)
{
	UNUSED(L);
	UNUSED(frame);
	UNUSED(cframe);

	return ERRF_FIND_CONTINUE;
}

/*
 * FRAME_CP:
 *  * Reached the 1st coroutine frame: No error function here, stop searching.
 *  * Not inherited error function in a "classic" FRAME_CP: found.
 *  * Otherwise: Continue searching.
 */
LJ_NOINLINE static ptrdiff_t throw_errf_find_frame_cp(const lua_State *L,
						      const TValue *frame,
						      const void *cframe)
{
	UNUSED(L);
	UNUSED(frame);
	ptrdiff_t errf;

	if (uj_cframe_canyield(cframe))
		return ERRF_FIND_NOT_FOUND;

	errf = (ptrdiff_t)uj_cframe_errfunc(cframe);
	if (errf >= ERRF_FIND_NOT_FOUND)
		return errf;

	return ERRF_FIND_CONTINUE;
}

/*
 * FRAME_PCALL, FRAME_PCALLH:
 *  * xpcall: found.
 *  * Otherwise: No error function here, stop searching (pcall does not
 *    propagate errors further along the stack).
 */
LJ_NOINLINE static ptrdiff_t throw_errf_find_frame_pcall(const lua_State *L,
							 const TValue *frame,
							 const void *cframe)
{
	UNUSED(cframe);

	/*
	 * In case of xpcall, [frame - 1] points to xpcall's error function
	 * by conventions.
	 */
	if (frame_ftsz(frame) >= (ptrdiff_t)(2 * sizeof(TValue)))
		return uj_state_stack_save(L, frame - 1);

	return ERRF_FIND_NOT_FOUND;
}

/* ORDER FRAME_ */
static const errf_find_callback errf_find_callbacks[] = {
	/* FRAME_LUA:    */ throw_errf_find_default,
	/* FRAME_C:      */ throw_errf_find_default,
	/* FRAME_CONT:   */ throw_errf_find_default,
	/* FRAME_VARG:   */ throw_errf_find_default,
	/* FRAME_LUAP:   */ throw_errf_find_default,
	/* FRAME_CP:     */ throw_errf_find_frame_cp,
	/* FRAME_PCALL:  */ throw_errf_find_frame_pcall,
	/* FRAME_PCALLH: */ throw_errf_find_frame_pcall};

/*
 * Looking for an error function in case cframe has no corresponding Lua frame,
 * i.e. when lj_vm_cpcall was used like a try statement in C++ (please search
 * the code base for concrete examples).
 */
static ptrdiff_t throw_errf_find_cpcall(const lua_State *L, const TValue *frame,
					const void **pcframe)
{
	int64_t nres;
	ptrdiff_t errf = ERRF_FIND_CONTINUE;
	const void *cframe = *pcframe;

	while ((nres = uj_cframe_nres(uj_cframe_raw(cframe))) < 0) {
		/*
		 * Native code invoked by lj_vm_cpcall constructed some Lua
		 * frames (e.g. during snap restoration): fallback to traversing
		 * Lua frames.
		 */
		if (frame >= uj_state_stack_restore(L, -nres))
			goto return_cpcall_errf;

		/* Error handler not inherited? */
		errf = (ptrdiff_t)uj_cframe_errfunc(cframe);
		if (errf >= ERRF_FIND_NOT_FOUND)
			goto return_cpcall_errf;

		/* Else unwind cframe and continue searching. */
		cframe = uj_cframe_prev(cframe);
		if (NULL == cframe) {
			errf = ERRF_FIND_NOT_FOUND;
			goto return_cpcall_errf;
		}
	}

return_cpcall_errf:
	*pcframe = cframe;
	return errf;
}

/* Find error function for runtime errors. Requires an extra stack traversal. */
static ptrdiff_t throw_errf_find(const lua_State *L)
{
	const TValue *frame = L->base - 1;
	const TValue *bottom = L->stack;
	const void *cframe = L->cframe;

	while (frame > bottom && cframe != NULL) {
		uint8_t frame_type;
		ptrdiff_t errf = throw_errf_find_cpcall(L, frame, &cframe);

		if (errf != ERRF_FIND_CONTINUE)
			return errf;

		frame_type = frame_type_unwind(frame);
		errf = errf_find_callbacks[frame_type](L, frame, cframe);

		if (errf != ERRF_FIND_CONTINUE)
			return errf;

		if (FRAME_C == frame_type)
			cframe = uj_cframe_prev(cframe);

		frame = frame_prev(frame);
	}

	return ERRF_FIND_NOT_FOUND;
}

static void throw_errf_call(struct lua_State *L, ptrdiff_t errf)
{
	const TValue *func = uj_state_stack_restore(L, errf);
	TValue *top = L->top;

	lua_assert(errf > 0);
	lj_trace_abort(G(L));

	if (!tvisfunc(func) || uj_state_has_event(L, EXTEV_ERRRUN_FUNC)) {
		uj_state_remove_event(L, EXTEV_ERRRUN_FUNC);
		setstrV(L, top - 1, uj_errmsg_str(L, UJ_ERR_ERRERR));
		uj_throw(L, LUA_ERRERR);
	}

	copyTV(L, top, top - 1);
	copyTV(L, top - 1, func);
	L->top = top + 1;

	uj_state_add_event(L, EXTEV_ERRRUN_FUNC);
	lj_vm_call(L, top, 1 + 1); /* Stack: |func|msg| -> |msg| */
	uj_state_remove_event(L, EXTEV_ERRRUN_FUNC);
}

LJ_NOINLINE void uj_throw_run(struct lua_State *L)
{
	ptrdiff_t errf = throw_errf_find(L);

	lua_assert(errf != ERRF_FIND_CONTINUE);

	if (errf != ERRF_FIND_NOT_FOUND)
		throw_errf_call(L, errf);

	uj_throw(L, LUA_ERRRUN);
}

LJ_NOINLINE void uj_throw_timeout(struct lua_State *L)
{
	uj_state_stack_sync_top(L);

	/*
	 * If you check for timeouts in function prologues (*FUNC* byte codes),
	 * ensure that you do this after the stack is checked for enough space
	 * for function's local variables. Misplacing a timeout check may lead
	 * to incorrect fix-up of the top of the stack with eventual OOB access.
	 */
	lua_assert(L->top <= L->maxstack);

	/*
	 * PROFILER: We are about to destroy the stack of running coroutine,
	 * so let's forbid any attempts to access it asynchronously.
	 */
	uj_vmstate_set(&G(L)->vmstate, UJ_VMST_INTERP);

	if (L->timeout.callback != NULL) {
		/* Unable to re-throw during error function execution: */
		lua_assert(!uj_state_has_event(L, EXTEV_TIMEOUT_FUNC));

		lj_trace_abort(G(L));

		uj_state_add_event(L, EXTEV_TIMEOUT_FUNC);
		L->timeout.nres = (L->timeout.callback)(L);
		uj_state_remove_event(L, EXTEV_TIMEOUT_FUNC);
	}

	uj_throw(L, LUAE_TIMEOUT);
}
