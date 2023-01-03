/*
 * Implementation of throwing run-time errors.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "lj_obj.h"
#include "uj_errmsg.h"
#include "uj_throw.h"
#include "uj_str.h"
#include "lj_debug.h"
#include "uj_state.h"
#include "lj_frame.h"

#if LJ_HASFFI
#include "uj_cframe.h"
#include "uj_ff.h"
#endif /* LJ_HASFFI */

static LJ_NORET void err_msg(struct lua_State *L, const char *em,
			     const char *arg1)
{
	const char *msg;

	uj_state_stack_sync_top(L);
	msg = uj_str_pushf(L, em, arg1);
	lj_debug_addloc(L, msg, L->base - 1, NULL);
	uj_throw_run(L);
}

UJ_ERRRUN void uj_err(struct lua_State *L, enum err_msg em)
{
	err_msg(L, "%s", uj_errmsg(em));
}

UJ_ERRRUN void uj_err_gco(struct lua_State *L, enum err_msg em, const GCobj *o)
{
	err_msg(L, uj_errmsg(em), uj_obj_itypename[o->gch.gct]);
}

#if LJ_HASFFI
/* Remove frame for FFI metamethods. */
static void err_remove_frame(struct lua_State *L, TValue *pframe, TValue *frame)
{
	const uint8_t ffid = frame_func(frame)->c.ffid;
	const int is_meta = ffid >= FF_ffi_meta___index &&
			    ffid <= FF_ffi_meta___tostring;

	if (!is_meta)
		return;

	L->base = pframe + 1;
	L->top = frame;
	uj_cframe_pc_set(uj_cframe_raw(L->cframe), frame_contpc(frame));
}
#endif /* LJ_HASFFI */

UJ_ERRRUN void uj_err_msg_caller(struct lua_State *L, const char *msg)
{
	TValue *frame = L->base - 1;
	TValue *pframe = NULL;

	if (frame_islua(frame)) {
		pframe = frame_prevl(frame);
	} else if (frame_iscont(frame)) {
#if LJ_HASFFI
		if (frame_is_cont_ffi_cb(frame)) {
			pframe = frame;
			frame = NULL;
		} else {
			pframe = frame_prevd(frame);
			err_remove_frame(L, pframe, frame);
		}
#else /* !LJ_HASFFI */
		pframe = frame_prevd(frame);
#endif /* LJ_HASFFI */
	}

	lj_debug_addloc(L, msg, pframe, frame);
	uj_throw_run(L);
}

UJ_ERRRUN void uj_err_callerv_(struct lua_State *L, const char *em, ...)
{
	const char *msg;

	va_list argp;
	va_start(argp, em);
	msg = uj_str_pushvf(L, em, argp);
	va_end(argp);
	uj_err_msg_caller(L, msg);
}

UJ_ERRRUN void uj_err_caller(struct lua_State *L, enum err_msg em)
{
	uj_err_msg_caller(L, uj_errmsg(em));
}

UJ_ERRRUN void uj_err_msg_arg(struct lua_State *L, const char *msg, int narg)
{
	const char *fname = "?";
	const char *ftype = lj_debug_funcname(L, L->base - 1, &fname);

	if (narg < 0 && narg > LUA_REGISTRYINDEX)
		narg = (int)(L->top - L->base) + narg + 1;

	if (ftype && ftype[3] == 'h' && --narg == 0) { /* Check for "method". */
		enum err_msg em = UJ_ERR_BADSELF;
		msg = uj_str_pushf(L, uj_errmsg(em), fname, msg);
	} else {
		enum err_msg em = UJ_ERR_BADARG;
		msg = uj_str_pushf(L, uj_errmsg(em), narg, fname, msg);
	}

	uj_err_msg_caller(L, msg);
}

UJ_ERRRUN void uj_err_argv(struct lua_State *L, enum err_msg em, int narg, ...)
{
	const char *msg;

	va_list argp;
	va_start(argp, narg);
	msg = uj_str_pushvf(L, uj_errmsg(em), argp);
	va_end(argp);
	uj_err_msg_arg(L, msg, narg);
}

UJ_ERRRUN void uj_err_arg(struct lua_State *L, enum err_msg em, int narg)
{
	uj_err_msg_arg(L, uj_errmsg(em), narg);
}

UJ_ERRRUN void uj_err_argtype(struct lua_State *L, int narg, const char *xname)
{
	const char *tname;
	const char *msg;

	if (narg <= LUA_REGISTRYINDEX) {
		if (narg >= LUA_GLOBALSINDEX) {
			tname = uj_obj_itypename[~LJ_TTAB];
		} else {
			const GCfunc *fn = curr_func(L);
			int idx = LUA_GLOBALSINDEX - narg;

			if (idx <= fn->c.nupvalues)
				tname = lj_typename(&fn->c.upvalue[idx - 1]);
			else
				tname = uj_obj_typename[0];
		}
	} else {
		TValue *tv = narg < 0 ? L->top + narg : L->base + narg - 1;
		tname = tv < L->top ? lj_typename(tv) : uj_obj_typename[0];
	}

	msg = uj_str_pushf(L, uj_errmsg(UJ_ERR_BADTYPE), xname, tname);
	uj_err_msg_arg(L, msg, narg);
}

UJ_ERRRUN void uj_err_argt(struct lua_State *L, int narg, int tt)
{
	uj_err_argtype(L, narg, uj_obj_typename[tt + 1]);
}
