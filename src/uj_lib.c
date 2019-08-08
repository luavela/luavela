/*
 * Library function support.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "lua.h"
#include "lauxlib.h"

#include "lj_obj.h"
#include "uj_err.h"
#include "uj_str.h"
#include "lj_gc.h"
#include "lj_tab.h"
#include "uj_func.h"
#include "uj_dispatch.h"
#include "uj_lib.h"

/* -- Library initialization ---------------------------------------------- */

struct reg_state {
	GCtab *tab; /* table of the lib being registered (e.g. _G.math). */
	GCtab *env; /* environment to create builtins with. */
	GCfunc *ofn; /* the most recently registered builtin. */
	BCIns *bcff; /* next free BC for fast functions. */
	const lua_CFunction *cf; /* module's lj_lib_cf_* (see lj_libdef.h). */
	const uint8_t *p; /* module's lj_lib_init_* (see lj_libdef.h). */
	size_t len; /* metadata from current pos in lj_lib_init_*. */
	uint32_t tag; /* metadata from current pos in lj_lib_init_*. */
	int ffid; /* next free fast function ID. */
};

void uj_lib_emplace(lua_State *L, const char *libname, int hsize)
{
	lua_assert(NULL != libname);

	luaL_findtable(L, LUA_REGISTRYINDEX, "_LOADED", 16);
	lua_getfield(L, -1, libname);
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		if (luaL_findtable(L, LUA_GLOBALSINDEX, libname, hsize) != NULL)
			uj_err_callerv(L, UJ_ERR_BADMODN, libname);
		lua_pushvalue(L, -1);
		lua_setfield(L, -3, libname); /* _LOADED[libname] = new table */
	}
	lua_remove(L, -2); /* Remove _LOADED table from the stack. */
}

static GCtab *lib_create_tab(lua_State *L, const char *libname, int hsize)
{
	GCtab *tab;

	if (libname)
		uj_lib_emplace(L, libname, hsize);
	else
		lua_createtable(L, 0, hsize);

	tab = tabV(L->top - 1);
	/* Avoid barriers further down. */
	lj_gc_anybarriert(L, tab);
	tab->nomm = 0;

	return tab;
}

static LJ_AINLINE GCstr *lib_create_str(lua_State *L, struct reg_state *rs)
{
	GCstr *str = uj_str_new(L, (const char *)rs->p, rs->len);

	rs->p += rs->len;
	return str;
}

static GCfunc *lib_create_func(lua_State *L, struct reg_state *rs,
			       ptrdiff_t tpos)
{
	size_t nuv = (size_t)(L->top - L->base - tpos);
	GCfunc *fn = uj_func_newC(L, nuv, rs->env);

	if (nuv) {
		L->top = L->base + tpos;
		memcpy(fn->c.upvalue, L->top, sizeof(TValue) * nuv);
	}

	fn->c.ffid = (uint8_t)rs->ffid;
	rs->ffid++;
	/*
	 * pc ("body" of the function as a GCfunc object):
	 * LIBINIT_CF (builtin is implemented fully in C): Set BC_FUNCC
	 * as function's "body" (actual invocation of the implementation is a
	 * part of the BC_FUNCC semantics).
	 * Otherwise (builtin is implemented as a fast function): Consume
	 * next free pseudo-BC for fast functions and set it as function's
	 * "body".
	 *
	 * f (actual implementation or its part):
	 * LIBINIT_ASM_ (builtin is implemented as a fast function and shares
	 * fallback part with the last declared LJLIB_ASM function): Inherit
	 * implementation from the previously processed function.
	 * Otherwise (builtin is implemented fully in C): Consume next
	 * implementation from the array of functions (see
	 * static const lua_CFunction lj_lib_cf_* in lj_libdef.h).
	 */
	if (rs->tag == LIBINIT_ASM_ && NULL == rs->ofn) {
		uj_err(L, UJ_ERR_BADFREG);
		return NULL; /* unreachable */
	}

	fn->c.pc = (rs->tag == LIBINIT_CF) ? &G(L)->bc_cfunc_int : (rs->bcff)++;
	fn->c.f = (rs->tag == LIBINIT_ASM_) ? rs->ofn->c.f : *(rs->cf)++;

	if (rs->len) {
		GCstr *name = lib_create_str(L, rs);
		/* NOBARRIER: See lib_create_tab for common barrier. */
		setfuncV(L, lj_tab_setstr(L, rs->tab, name), fn);
	}

	return fn;
}

static int lib_register_misc(lua_State *L, struct reg_state *rs)
{
	switch (rs->tag | rs->len) {
	case LIBINIT_SET: {
		/*
		 * Set a value from the top of the stack as a new env
		 * OR store a kv-pair into the lib table.
		 */
		L->top -= 2;
		if (tvisstr(L->top + 1) && strV(L->top + 1)->len == 0)
			rs->env = tabV(L->top);
		else /* NOBARRIER: See lib_create_tab for common barrier. */
			copyTV(L, lj_tab_set(L, rs->tab, L->top + 1), L->top);
		break;
	}
	case LIBINIT_NUMBER: {
		/* Read a double from lj_lib_init_* and push it on the stack. */
		double tmp;

		memcpy(&tmp, rs->p, sizeof(tmp));
		rs->p += sizeof(tmp);
		setnumV(L->top, tmp);
		L->top++;
		break;
	}
	case LIBINIT_COPY: {
		/*
		 * lua_pushvalue(L, index) so that a value could be used as
		 * an upvalue for the next builtin.
		 */
		copyTV(L, L->top, L->top - *(rs->p)++);
		L->top++;
		break;
	}
	case LIBINIT_LASTCL: {
		/*
		 * Copy the most recently registered builtin on stack so that
		 * it could be used as an upvalue for the next builtin.
		 */
		if (NULL == rs->ofn) {
			uj_err(L, UJ_ERR_BADFREG);
			break; /* unreachable */
		}
		setfuncV(L, L->top++, rs->ofn);
		break;
	}
	case LIBINIT_FFID: {
		/* Dummy-consume ffid for non-registered builtins. */
		(rs->ffid)++;
		break;
	}
	case LIBINIT_END: {
		return 1; /* End of lj_lib_init_* */
	}
	default: {
		/* Read a string from lj_lib_init_* and push it on the stack. */
		setstrV(L, L->top++, lib_create_str(L, rs));
		break;
	}
	}
	return 0;
}

void uj_lib_register(lua_State *L, const char *libname, const uint8_t *p,
		     const lua_CFunction *cf)
{
	int ffid = *p++;
	BCIns *bcff = &L2GG(L)->bcff[*p++];
	GCtab *tab = lib_create_tab(L, libname, *p++);
	const ptrdiff_t tpos = L->top - L->base;
	struct reg_state rs = {0};

	rs.tab = tab;
	rs.env = L->env;
	rs.cf = cf;
	rs.bcff = bcff;
	rs.p = p;
	rs.ffid = ffid;

	for (;;) {
		rs.tag = *(rs.p)++;
		rs.len = rs.tag & LIBINIT_LENMASK;
		rs.tag &= LIBINIT_TAGMASK;

		if (rs.tag != LIBINIT_STRING) {
			rs.ofn = lib_create_func(L, &rs, tpos);
		} else {
			if (lib_register_misc(L, &rs) != 0)
				return;
		}
	}
}

/* -- Type checks --------------------------------------------------------- */

GCstr *uj_lib_checkstr(lua_State *L, unsigned int narg)
{
	TValue *tv = uj_lib_narg2tv(L, narg);
	GCstr *s;

	if (tv >= L->top || !uj_str_is_coercible(tv))
		uj_err_argt(L, narg, LUA_TSTRING);

	if (LJ_LIKELY(tvisstr(tv)))
		return strV(tv);

	lua_assert(tvisnum(tv));
	s = uj_str_fromnumber(L, tv->n);
	setstrV(L, tv, s);
	return s;
}

GCtab *uj_lib_checktabornil(lua_State *L, unsigned int narg)
{
	const TValue *tv = uj_lib_narg2tv(L, narg);

	if (tv < L->top) {
		if (tvistab(tv))
			return tabV(tv);
		else if (tvisnil(tv))
			return NULL;
	}
	uj_err_arg(L, UJ_ERR_NOTABN, narg);
	return NULL; /* unreachable */
}

int uj_lib_checkopt(lua_State *L, unsigned int narg, int def, const char *lst)
{
	const char *opt;
	size_t optlen;
	uint8_t len;
	int i;

	const GCstr *s = def >= 0 ? uj_lib_optstr(L, narg) :
				    uj_lib_checkstr(L, narg);

	if (NULL == s)
		return def;

	opt = strdata(s);
	optlen = s->len;
	for (i = 0; 0 != (len = *(const uint8_t *)lst); i++) {
		if (len == optlen && memcmp(opt, lst + 1, optlen) == 0)
			return i;
		lst += 1 + len;
	}
	uj_err_argv(L, UJ_ERR_INVOPTM, narg, opt);
}
