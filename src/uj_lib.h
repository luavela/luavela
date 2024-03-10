/*
 * Library function support.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _UJ_LIB_H
#define _UJ_LIB_H

#include <stdio.h>

#include "lua.h"

#include "lj_obj.h"
#include "uj_err.h"
#include "uj_str.h"

/*
 * A fallback handler is called by the assembler VM if the fast path fails:
 *
 * - too few arguments:   unrecoverable.
 * - wrong argument type:   recoverable, if coercion succeeds.
 * - bad argument value:  unrecoverable.
 * - stack overflow:        recoverable, if stack reallocation succeeds.
 * - extra handling:        recoverable.
 *
 * The unrecoverable cases throw an error with uj_err_arg(), uj_err_argtype(),
 * uj_err_caller() or uj_err_msg_caller().
 * The recoverable cases return 0 or the number of results + 1.
 * The assembler VM retries the fast path only if 0 is returned.
 * This time the fallback must not be called again or it gets stuck in a loop.
 */

/* Return values from fallback handler. */
#define FFH_RETRY 0
#define FFH_UNREACHABLE FFH_RETRY
#define FFH_RES(n) ((n) + 1)
#define FFH_TAILCALL (-1)

/* -- Library initialization ---------------------------------------------- */

/*
 * Based on the libname parameter, prepares a proper table for the library.
 * Leaves it on top of the stack, or throws in case of error.
 */
void uj_lib_emplace(lua_State *L, const char *libname, int hsize);

/*
 * Based on data read from init and the list of implementations cf
 * registers all builtins for library libname.
 */
void uj_lib_register(lua_State *L, const char *libname, const uint8_t *init,
		     const lua_CFunction *cf);

#define LJ_LIB_REG(L, regname, name) \
	uj_lib_register(L, regname, lj_lib_init_##name, lj_lib_cf_##name)

/* Library init data tags. */
#define LIBINIT_LENMASK 0x3f
#define LIBINIT_TAGMASK 0xc0
#define LIBINIT_CF 0x00
#define LIBINIT_ASM 0x40
#define LIBINIT_ASM_ 0x80
#define LIBINIT_STRING 0xc0
#define LIBINIT_MAXSTR 0x39
#define LIBINIT_SET 0xfa
#define LIBINIT_NUMBER 0xfb
#define LIBINIT_COPY 0xfc
#define LIBINIT_LASTCL 0xfd
#define LIBINIT_FFID 0xfe
#define LIBINIT_END 0xff

/* Library function declarations. Scanned by buildvm. */
#define LJLIB_CF(name) static int lj_cf_##name(lua_State *L)
#define LJLIB_ASM(name) static int lj_ffh_##name(lua_State *L)
#define LJLIB_ASM_(name)
#define LJLIB_SET(name)
#define LJLIB_PUSH(arg)
#define LJLIB_REC(handler)
#define LJLIB_NOREGUV
#define LJLIB_NOREG

/* -- Type checks --------------------------------------------------------- */

static LJ_AINLINE TValue *uj_lib_narg2tv(lua_State *L, unsigned int narg)
{
	lua_assert(narg > 0);
	return L->base + narg - 1;
}

static LJ_AINLINE TValue *uj_lib_checkany(lua_State *L, unsigned int narg)
{
	TValue *tv = uj_lib_narg2tv(L, narg);

	if (tv >= L->top)
		uj_err_arg(L, UJ_ERR_NOVAL, narg);
	return tv;
}

GCstr *uj_lib_checkstr(lua_State *L, unsigned int narg);

static LJ_AINLINE GCstr *uj_lib_optstr(lua_State *L, unsigned int narg)
{
	const TValue *tv = uj_lib_narg2tv(L, narg);

	return (tv < L->top && !tvisnil(tv)) ? uj_lib_checkstr(L, narg) : NULL;
}

static LJ_AINLINE lua_Number uj_lib_checknum(lua_State *L, unsigned int narg)
{
	TValue *tv = uj_lib_narg2tv(L, narg);

	if (!(tv < L->top && uj_str_tonumber(tv)))
		uj_err_argt(L, narg, LUA_TNUMBER);
	return numV(tv);
}

static LJ_AINLINE int32_t uj_lib_checkint(lua_State *L, unsigned int narg)
{
	return lj_num2int(uj_lib_checknum(L, narg));
}

static LJ_AINLINE int32_t uj_lib_optint(lua_State *L, unsigned int narg,
					int32_t def)
{
	const TValue *tv = uj_lib_narg2tv(L, narg);

	return (tv < L->top && !tvisnil(tv)) ? uj_lib_checkint(L, narg) : def;
}

static LJ_AINLINE int uj_lib_optbool(lua_State *L, int narg)
{
	const TValue *tv = uj_lib_narg2tv(L, narg);

	return (tv < L->top && tvistruecond(tv)) ? 1 : 0;
}

static LJ_AINLINE int32_t uj_lib_checkbit(lua_State *L, unsigned int narg)
{
	return lj_num2bit(uj_lib_checknum(L, narg));
}

static LJ_AINLINE GCfunc *uj_lib_checkfunc(lua_State *L, unsigned int narg)
{
	const TValue *tv = uj_lib_narg2tv(L, narg);

	if (!(tv < L->top && tvisfunc(tv)))
		uj_err_argt(L, narg, LUA_TFUNCTION);
	return funcV(tv);
}

static LJ_AINLINE GCtab *uj_lib_checktab(lua_State *L, unsigned int narg)
{
	const TValue *tv = uj_lib_narg2tv(L, narg);

	if (!(tv < L->top && tvistab(tv)))
		uj_err_argt(L, narg, LUA_TTABLE);
	return tabV(tv);
}

static LJ_AINLINE GCtab *uj_lib_opttab(lua_State *L, unsigned int narg)
{
	const TValue *tv = uj_lib_narg2tv(L, narg);

	return (tv < L->top && !tvisnil(tv)) ? uj_lib_checktab(L, narg) : NULL;
}

static LJ_AINLINE GCudata *uj_lib_checkiofile(lua_State *L, unsigned int narg)
{
	const TValue *tv = uj_lib_narg2tv(L, narg);

	if (!(tv < L->top && tvislibiofile(tv)))
		uj_err_argt(L, narg, LUA_TUSERDATA);
	return udataV(tv);
}

GCtab *uj_lib_checktabornil(lua_State *L, unsigned int narg);

int uj_lib_checkopt(lua_State *L, unsigned int narg, int def, const char *lst);

/* -- Structures and defines shared across the standard library ----------- */

/* Avoid including lj_frame.h. */
static LJ_AINLINE TValue *uj_lib_upvalue(const lua_State *L, int n)
{
	return &((L->base - 1)->fr.func->fn.c.upvalue[n - 1]);
}

/* Push internal function on the stack. */
static LJ_AINLINE void uj_lib_pushcc(lua_State *L, lua_CFunction f, int id,
				     int n)
{
	GCfunc *fn;

	lua_pushcclosure(L, f, n);
	fn = funcV(L->top - 1);
	fn->c.ffid = (uint8_t)id;
	fn->c.pc = &G(L)->bc_cfunc_int;
}

static LJ_AINLINE void uj_lib_pushcf(lua_State *L, lua_CFunction f, int id)
{
	uj_lib_pushcc(L, f, id, 0);
}

struct RandomState;
uint64_t lj_math_random_step(struct RandomState *rs);

/* Userdata payload for I/O file. */
struct IOFileUD {
	FILE *fp; /* File handle. */
	uint32_t type; /* File type. */
};

#define IOFILE_TYPE_FILE 0 /* Regular file. */
#define IOFILE_TYPE_PIPE 1 /* Pipe. */
#define IOFILE_TYPE_STDF 2 /* Standard file handle. */
#define IOFILE_TYPE_MASK 3

#define IOFILE_FLAG_CLOSE 4 /* Close after io.lines() iterator. */

#endif /* !_UJ_LIB_H */
