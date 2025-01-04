/*
 * Metamethod handling.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 *
 * Portions taken verbatim or adapted from the Lua interpreter.
 * Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
 */

#ifndef _UJ_META_H
#define _UJ_META_H

#include "lj_obj.h"

LJ_DATA const uint8_t uj_mm_narg[];

#define MM_NARG_ARITH uj_mm_narg[MM_add]
#define MM_NARG_COMP uj_mm_narg[MM_eq]

/* Initialise per-VM data related to metamethods. */
void uj_meta_init(lua_State *L);

/* Return the name of the metamethod mm. */
static LJ_AINLINE GCstr *uj_meta_name(const global_State *g, enum MMS mm)
{
	return gco2str(g->gcroot[GCROOT_MMNAME + mm]);
}

/*
 * Performs a lookup of metamethod mm for tv.
 * Returns the metamethod or nil if no metatable or metamethod was found.
 */
const TValue *uj_meta_lookup(const lua_State *L, const TValue *tv, enum MMS mm);

/*
 * Performs a lookup of metamethod mm in table mt. If mm was not found, caches
 * this result in mt's internal "nomm" cache and returns NULL.
 * Otherwise returns the metamethod.
 */
const TValue *uj_meta_lookup_mt(const global_State *g, GCtab *mt, enum MMS mm);

/* C helpers for some instructions, called from assembler VM. */

const TValue *uj_meta_tget(lua_State *L, const TValue *tv, const TValue *k);

TValue *uj_meta_tset(lua_State *L, const TValue *tv, const TValue *k);

TValue *uj_meta_arith(lua_State *L, TValue *ra, const TValue *rb,
		      const TValue *rc, BCReg op);

TValue *uj_meta_len(lua_State *L, const TValue *tv);

TValue *uj_meta_equal(lua_State *L, GCobj *o1, GCobj *o2, int ne);

TValue *uj_meta_comp(lua_State *L, const TValue *tv1, const TValue *tv2,
		     int op);

void uj_meta_call(lua_State *L, TValue *func, TValue *top);

void uj_meta_for(lua_State *L, TValue *tv);

/*
 * Helper function for the CAT bytecode semantics. Performs a single
 * concatenation step. If all slots from `bottom` to `top` (both inclusive)
 * could be concatenated during this step, stores result in `bottom` and
 * returns NULL. Otherwise prepares the stack for a __concat metamethod call
 * and returns the metamethod base.
 */
TValue *uj_meta_cat(lua_State *L, TValue *bottom, TValue *top);

void uj_meta_mmcall(lua_State *L, TValue *newbase, int nargs, int nres);

/*
 * Fetches a value through a __index metamethod call.
 * Assumes that the metamethod frame is set up at the top of L.
 */
const TValue *uj_meta_index(lua_State *L);

/*
 * Recursively indexes the value at L->base with the rest of n - 1 arguments.
 * Respects metamethods. Also see lj_tab_rawrindex.
 */
const TValue *uj_meta_rindex(lua_State *L, size_t n);

#if LJ_HASFFI

int uj_meta_tailcall(lua_State *L, const TValue *tv);

TValue *uj_meta_equal_cd(lua_State *L, BCIns ins);

#endif /* LJ_HASFFI */

#endif /* !_UJ_META_H */
