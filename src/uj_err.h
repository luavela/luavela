/*
 * Convenience interfaces for throwing run-time errors. Variadic versions of the
 * interfaces are used for producing formatted error messages.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _UJ_ERR_H
#define _UJ_ERR_H

#include "lj_def.h"
#include "uj_errmsg.h"

struct lua_State;
union GCobj;

/*
 * Basic interfaces for throwing errors.
 * Primarily used to throw from inside the VM.
 */

/* Throw a run-time error em. */
UJ_ERRRUN void uj_err(struct lua_State *L, enum err_msg em);

/* Throw a run-time error em with some extra info about object o. */
UJ_ERRRUN void uj_err_gco(struct lua_State *L, enum err_msg em, const GCobj *o);

/*
 * Throwing errors in the context of caller.
 * Primarily used by library functions.
 */

/* Throw a run-time error em in the context of caller. */
UJ_ERRRUN void uj_err_caller(struct lua_State *L, enum err_msg em);

/*
 * Throw a run-time error em in the context of caller, variadic version.
 * NB! Using a macro preserves signature consistency across the set of
 * interfaces and works around undefined behaviour issue with the arg of
 * enum type preceding the variadic arguments.
 */
UJ_ERRRUN void uj_err_callerv_(struct lua_State *L, const char *em, ...);
#define uj_err_callerv(L, /* enum err_msg */ em, ...) \
	uj_err_callerv_((L), uj_errmsg(em), __VA_ARGS__)

/* Throw a run-time error msg in the context of caller. */
UJ_ERRRUN void uj_err_msg_caller(struct lua_State *L, const char *msg);

/* Throwing argument errors. Primarily used by library functions. */

/* Throw a run-time nargs-th argument error em. */
UJ_ERRRUN void uj_err_arg(struct lua_State *L, enum err_msg em, int narg);

/* Throw a run-time nargs-th argument error em, variadic version. */
UJ_ERRRUN void uj_err_argv(struct lua_State *L, enum err_msg em, int narg, ...);

/* Throw a run-time nargs-th argument error msg. */
UJ_ERRRUN void uj_err_msg_arg(struct lua_State *L, const char *msg, int narg);

/*
 * Typecheck errors for the narg-th argument on the stack.
 * Primarily used by library functions.
 */

/*
 * Throw a typecheck run-time error for the the narg-th argument.
 * The type is denoted by the numeric tag tt.
 */
UJ_ERRRUN void uj_err_argt(struct lua_State *L, int narg, int tt);

/*
 * Throw a typecheck run-time error for the the narg-th argument.
 * The type is denoted by string xname.
 */
UJ_ERRRUN void uj_err_argtype(struct lua_State *L, int narg, const char *xname);

#endif /* !_UJ_ERR_H */
