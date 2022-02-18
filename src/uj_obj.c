/*
 * Miscellaneous object handling.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "uj_mem.h"
#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_vm.h"
#include "uj_throw.h"
#include "lj_tab.h"
#include "uj_proto.h"
#include "uj_func.h"
#include "uj_str.h"

/* Object type names. */
LJ_DATADEF const char *const uj_obj_typename[] = {/* ORDER LUA_T */
						  "no value", "nil",
						  "boolean",  "userdata",
						  "number",   "string",
						  "table",    "function",
						  "userdata", "thread",
						  "proto",    "cdata"};

LJ_DATADEF const char *const uj_obj_itypename[] = {/* ORDER LJ_T */
						   "nil",      "boolean",
						   "boolean",  "userdata",
						   "string",   "upval",
						   "thread",   "proto",
						   "function", "trace",
						   "cdata",    "table",
						   "userdata", "number"};

void *uj_obj_new(lua_State *L, size_t size)
{
	global_State *g = G(L);
	GCobj *o = uj_mem_alloc(L, size);
	lua_assert(o != NULL);

	o->gch.nextgc = g->gc.root;
	g->gc.root = o;
	newwhite(g, o);
	return o;
}

/* Compare two objects without calling metamethods. */
int uj_obj_equal(const TValue *o1, const TValue *o2)
{
	if (gettag(o1) == gettag(o2)) {
		if (tvispri(o1))
			return 1;
		if (!tvisnum(o1))
			return gcval(o1) == gcval(o2);
	}
	if (!tvisnum(o1) || !tvisnum(o2))
		return 0;
	return numV(o1) == numV(o2);
}

static const GCtab *obj_map_get(lua_State *L, GCtab *map, GCtab *key)
{
	TValue keytv;
	const TValue *valuetv;

	settabV(L, &keytv, key);
	valuetv = lj_tab_get(L, map, &keytv);
	lua_assert(valuetv != NULL);
	return tabV(valuetv);
}

GCobj *uj_obj_deepcopy(lua_State *L, GCobj *src, struct deepcopy_ctx *ctx)
{
	if (lj_obj_has_mark(src) && (src->gch.gct == ~LJ_TTAB)) {
		if (ctx->map == NULL) /* copy pointer to visited table */
			return src;
		else /* copy pointer to already copied visited table */
			return obj2gco(obj_map_get(L, ctx->map, gco2tab(src)));
	}

	switch (src->gch.gct) {
	case (~LJ_TTAB): {
		GCobj *o = obj2gco(lj_tab_deepcopy(L, gco2tab(src), ctx));

		lua_assert(!uj_obj_is_sealed(o));
		lua_assert(!uj_obj_is_immutable(o));

		return o;
	}
	case (~LJ_TSTR): {
		GCobj *o = obj2gco(uj_str_copy(L, gco2str(src)));

		/* No sealing assumptions for strings. */
		lua_assert(uj_obj_is_immutable(o));

		return o;
	}
	case (~LJ_TFUNC): {
		const GCproto *pt;
		const GCfunc *fn = gco2func(src);
		GCproto *dst;
		GCobj *o;

		lua_assert(isluafunc(fn));
		pt = funcproto(fn);

		dst = uj_proto_deepcopy(L, pt, ctx);

		lua_assert(!uj_obj_is_sealed(obj2gco(dst)));
		lua_assert(uj_obj_is_immutable(obj2gco(dst)));

		o = obj2gco(uj_func_newL_empty(L, dst, ctx->env));

		lua_assert(!uj_obj_is_sealed(o));
		lua_assert(uj_obj_is_immutable(o));

		return o;
	}
	default: /* NYI */
		lua_assert(0);
		return NULL;
	}
}

/* Clear marks after deep copy */
void uj_obj_clear_mark(lua_State *L, GCobj *o)
{
	if (!lj_obj_has_mark(o))
		return;
	if (o->gch.gct == ~LJ_TSTR) /* skip fixed strings */
		return;
	lua_assert(o->gch.gct == ~LJ_TTAB);
	lj_obj_clear_mark(o);
	lj_tab_traverse(L, gco2tab(o), uj_obj_clear_mark);
}

struct pmark_context {
	gco_mark_flipper marker;
	GCobj *o;
};

/* Wrapper to start marking of an object in a protected mode. */
static TValue *obj_mark(lua_State *L, lua_CFunction dummy, void *ud)
{
	UNUSED(dummy);
	struct pmark_context *ctx = ud;
	lua_assert(ctx->marker != NULL);
	lua_assert(ctx->o != NULL);
	(ctx->marker)(L, ctx->o);
	return NULL;
}

void uj_obj_pmark(lua_State *L, GCobj *o, gco_mark_flipper marker,
		  gco_mark_flipper rubber)
{
	struct pmark_context ctx = {.marker = marker, .o = o};

	int status = lj_vm_cpcall(L, NULL, &ctx, obj_mark);
	if (status == 0)
		return;

	lua_assert(status == LUA_ERRRUN);
	lua_assert(rubber != NULL);
	rubber(L, o);
	uj_throw(L, status); /* Propagate errors. */
}

void uj_obj_propagate_set(lua_State *L, GCobj *o, gco_traverser traverser,
			  gco_mark_flipper marker)
{
	if (lj_obj_has_mark(o))
		return;
	lua_assert(traverser != NULL);
	lua_assert(marker != NULL);
	lj_obj_set_mark(o);
	traverser(L, o, marker);
}

void uj_obj_propagate_clear(lua_State *L, GCobj *o, gco_traverser traverser,
			    gco_mark_flipper rubber)
{
	if (!lj_obj_has_mark(o))
		return;
	lua_assert(traverser != NULL);
	lua_assert(rubber != NULL);
	lj_obj_clear_mark(o);
	traverser(L, o, rubber);
}
