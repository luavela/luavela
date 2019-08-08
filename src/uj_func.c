/*
 * Function handling
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 *
 * Portions taken verbatim or adapted from the Lua interpreter.
 * Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
 */

#include "lj_obj.h"
#include "uj_obj_immutable.h"
#include "lj_gc.h"
#include "uj_mem.h"
#include "uj_proto.h"
#include "uj_func.h"
#include "uj_upval.h"

static size_t sizeCfunc(size_t nupvalues)
{
	return sizeof(GCfuncC) - sizeof(TValue) + sizeof(TValue) * nupvalues;
}

static size_t sizeLfunc(size_t nupvalues)
{
	return sizeof(GCfuncL) - sizeof(GCupval *) +
	       sizeof(GCupval *) * nupvalues;
}

static GCfunc *func_new(lua_State *L, size_t size)
{
	GCfunc *fn = (GCfunc *)uj_obj_new(L, size);
	uj_obj_immutable_set_mark(obj2gco(fn));
	return fn;
}

GCfunc *uj_func_newC(lua_State *L, size_t nelems, GCtab *env)
{
	GCfunc *fn = func_new(L, sizeCfunc(nelems));
	fn->c.gct = ~LJ_TFUNC;
	fn->c.ffid = FF_C;
	fn->c.nupvalues = (uint8_t)nelems;
	/* NOBARRIER: The GCfunc is new (marked white). */
	fn->c.pc = &G(L)->bc_cfunc_ext;
	fn->c.env = env;
	return fn;
}

static GCfunc *func_newL(lua_State *L, GCproto *pt, GCtab *env)
{
	GCfunc *fn = func_new(L, sizeLfunc(pt->sizeuv));
	fn->l.gct = ~LJ_TFUNC;
	fn->l.ffid = FF_LUA;
	fn->l.nupvalues = 0; /* Set to zero until upvalues are initialized. */
	/* NOBARRIER: Really a setgcref. But the GCfunc is new (marked white).*/
	fn->l.pc = proto_bc(pt);
	fn->l.env = env;
	uj_proto_count_closure(pt);
	return fn;
}

/* Create a new Lua function with empty upvalues. */
GCfunc *uj_func_newL_empty(lua_State *L, GCproto *pt, GCtab *env)
{
	GCfunc *fn = func_newL(L, pt, env);
	size_t i, nuv = (size_t)pt->sizeuv;
	/* NOBARRIER: The GCfunc is new (marked white). */
	for (i = 0; i < nuv; i++) {
		GCupval *uv = uj_upval_new_closed_empty(L);
		int32_t v = proto_uv(pt)[i];
		uv->immutable = ((v / PROTO_UV_IMMUTABLE) & 1);
		uv->dhash = uj_upval_dhash(pt, v);
		fn->l.uvptr[i] = uv;
	}
	fn->l.nupvalues = (uint8_t)nuv;
	return fn;
}

/* Do a GC check and create a new Lua function with inherited upvalues. */
GCfunc *uj_func_newL_gc(lua_State *L, GCproto *pt, GCfuncL *parent)
{
	GCfunc *fn;
	GCupval **puv;
	size_t i, nuv;
	TValue *base;
	lj_gc_check_fixtop(L);
	fn = func_newL(L, pt, parent->env);
	/* NOBARRIER: The GCfunc is new (marked white). */
	puv = parent->uvptr;
	nuv = pt->sizeuv;
	base = L->base;
	for (i = 0; i < nuv; i++) {
		uint32_t v = proto_uv(pt)[i];
		GCupval *uv;
		if ((v & PROTO_UV_LOCAL)) {
			uv = uj_upval_find(L, base + (ptrdiff_t)(v & 0xff));
			uv->immutable = ((v / PROTO_UV_IMMUTABLE) & 1);
			uv->dhash = uj_upval_dhash(parent->pc, v);
		} else {
			uv = puv[v];
		}
		fn->l.uvptr[i] = uv;
	}
	fn->l.nupvalues = (uint8_t)nuv;
	return fn;
}

int uj_func_usesfenv(const GCfunc *fn)
{
	/* Assume that built-in functions do not rely on fenv. */
	if (isffunc(fn))
		return 0;

	/* Assume that C fuctions rely on fenv (just to be safe). */
	if (iscfunc(fn))
		return 1;

	lua_assert(isluafunc(fn));

	if (0 != fn->l.nupvalues)
		return 1;

	return uj_proto_accesses_globals(funcproto(fn));
}

size_t uj_func_sizeof(const GCfunc *fn)
{
	if (isluafunc(fn))
		return sizeLfunc((size_t)fn->l.nupvalues);
	else
		return sizeCfunc((size_t)fn->c.nupvalues);
}

void uj_func_free(global_State *g, GCfunc *fn)
{
	uj_mem_free(MEM_G(g), fn, uj_func_sizeof(fn));
}

static void func_mark_proto(lua_State *L, GCfunc *fn, gco_mark_flipper marker)
{
	lua_assert(isluafunc(fn));
	marker(L, obj2gco(funcproto(fn)));
}

void uj_func_seal_traverse(lua_State *L, GCfunc *fn, gco_mark_flipper marker)
{
	if (!isluafunc(fn))
		return;

	func_mark_proto(L, fn, marker);
}
