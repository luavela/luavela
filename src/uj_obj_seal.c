/*
 * Implementation of sealing and unsealing.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "lj_obj.h"
#include "uj_obj_immutable.h"
#include "lj_tab.h"
#include "uj_proto.h"
#include "uj_func.h"
#include "uj_strhash.h"
#include "uj_err.h"

static LJ_AINLINE void seal_set_mark(GCobj *o)
{
	o->gch.marked |= UJ_GCO_SEALED;
}

static LJ_AINLINE void seal_clear_mark(GCobj *o)
{
	o->gch.marked &= (uint8_t)~UJ_GCO_SEALED;
}

static size_t seal_sizeof(const GCobj *o)
{
	switch (o->gch.gct) {
	case (~LJ_TTAB):
		return lj_tab_sizeof(gco2tab(o));
	case (~LJ_TPROTO):
		return uj_proto_sizeof(gco2pt(o));
	case (~LJ_TFUNC):
		return uj_func_sizeof(gco2func(o));
	default:
		/*
		 * Other types are either unsupported (coroutines, userdata,
		 * etc.) or not in the GC chain (strings), hence 0.
		 */
		return 0;
	}
	lua_assert(0);
	return 0;
}

/*
 * Sealing implies immutability. So we need to enforce this property
 * explicitly for objects that do not acquire it on creation. No need to do
 * this recursively: Fixup is run during relinking an object chain, so we
 * are guaranteed to touch each and every object that was reached by sealing.
 */
static LJ_AINLINE void seal_immutable_fixup(GCobj *o)
{
	if (o->gch.gct == ~LJ_TTAB)
		uj_obj_immutable_set_mark(o);
	lua_assert(uj_obj_is_immutable(o));
}

static void seal_relink_chain(GCobj **p)
{
	GCobj *o;
	GCobj *sealed_anchor = NULL;
	GCobj *last = NULL;

	while (NULL != (o = *p)) {
		if (uj_obj_is_sealed(o)) {
			seal_immutable_fixup(o);
			*p = gcnext(o);
			o->gch.nextgc = sealed_anchor;
			sealed_anchor = o;
		} else {
			last = o;
			p = &o->gch.nextgc;
		}
	}

	/*
	 * If there are non-sealed objects remaining in the chain,
	 * attach the sealed list to the last object. If no non-sealed objects
	 * are found, just re-create the chain. In this case, p never moved and
	 * points at the chain anchor.
	 */
	if (NULL != last)
		last->gch.nextgc = sealed_anchor;
	else
		*p = sealed_anchor;
}

static void seal_relink_root(global_State *g)
{
	seal_relink_chain(&g->gc.root);
}

#ifndef NDEBUG
static void seal_assert(global_State *g)
{
	GCobj *o = g->gc.root;
	while (o) {
		if (uj_obj_is_sealed(o))
			break;
		o = gcnext(o);
	}
	lua_assert(NULL == o || uj_obj_is_sealed(o));
	while (o) {
		lua_assert(uj_obj_is_sealed(o));
		lua_assert(uj_obj_is_immutable(o));
		lua_assert(o->gch.gct == ~LJ_TTAB || o->gch.gct == ~LJ_TPROTO ||
			   o->gch.gct == ~LJ_TFUNC || o->gch.gct == ~LJ_TSTR);
		o = gcnext(o);
	}
	lua_assert(NULL == o);
}
#else
static void seal_assert(global_State *g)
{
	UNUSED(g);
}
#endif /* !NDEBUG */

/*
 * Relinks GC chains in order to comply to the seal invariant:
 * sealed object can only point to another sealed object via nextgc.
 */
static void seal_relink(lua_State *L)
{
	global_State *g = G(L);
	seal_relink_root(g);
	if (!gl_datastate(g)) /* Otherwise there's nothing to seal. */
		uj_strhash_relink(gl_strhash(g), gl_strhash_sealed(g), L);
	seal_assert(g);
}

static void seal_mark(lua_State *L, GCobj *o);

/* Dispatches and throws an appropriate error message. */
static void seal_err(enum err_msg em, lua_State *L, GCobj *o)
{
	lua_assert(em == UJ_ERR_SEAL_BADTYPE || em == UJ_ERR_SEAL_FNUPVAL);
	if (em == UJ_ERR_SEAL_BADTYPE)
		uj_err_gco(L, em, o);
	else
		uj_err(L, em);
}

/*
 * Inspects an object and calls per-type traversing function.
 * Actual marking/unmarking is done by the marker callback. May throw
 * during marking, may throw. Does not throw during unmarking.
 */
static void seal_traverse(lua_State *L, GCobj *o, gco_mark_flipper marker)
{
	enum err_msg em = UJ_ERR__MAX;
	switch (o->gch.gct) {
	case (~LJ_TTAB): {
		lj_tab_traverse(L, gco2tab(o), marker);
		return;
	}
	case (~LJ_TPROTO): {
		uj_proto_seal_traverse(L, gco2pt(o), marker);
		return;
	}
	case (~LJ_TFUNC): {
		/* NYI: sealed functions with upvalues. */
		GCfunc *fn = gco2func(o);
		if (uj_func_has_upvalues(fn)) {
			em = UJ_ERR_SEAL_FNUPVAL;
			break;
		}
		uj_func_seal_traverse(L, fn, marker);
		return;
	}
	case (~LJ_TSTR): {
		lua_assert(0);
		return;
	}
	default: { /* NYI */
		lua_assert(
			o->gch.gct == ~LJ_TUPVAL || o->gch.gct == ~LJ_TTHREAD ||
			o->gch.gct == ~LJ_TTRACE || o->gch.gct == ~LJ_TCDATA ||
			o->gch.gct == ~LJ_TUDATA);
		em = UJ_ERR_SEAL_BADTYPE;
		break;
	}
	}
	if (marker == seal_mark)
		seal_err(em, L, o);
}

static void seal_mark(lua_State *L, GCobj *o)
{
	if (uj_obj_is_sealed(o))
		return;

	if (o->gch.gct == ~LJ_TSTR) {
		/*
		* All strings that are subject to sealing must already be
		* sealed in case DataState is used.
		*/
		lua_assert(!gl_datastate(G(L)));
		return; /* Avoid interference with LJ_GC_FIXED. */
	}

	uj_obj_propagate_set(L, o, seal_traverse, seal_mark);
}

static void seal_unmark(lua_State *L, GCobj *o)
{
	if (uj_obj_is_sealed(o))
		return;

	if (o->gch.gct == ~LJ_TSTR)
		return; /* Avoid interference with LJ_GC_FIXED. */

	uj_obj_propagate_clear(L, o, seal_traverse, seal_unmark);
}

static void seal_commit(lua_State *L, GCobj *o)
{
	size_t size = 0; /* Does not account strings! */

	if (uj_obj_is_sealed(o))
		return;

	seal_set_mark(o);

	if (o->gch.gct == ~LJ_TSTR)
		return; /* Avoid interference with LJ_GC_FIXED. */

	lua_assert(lj_obj_has_mark(o));
	lj_obj_clear_mark(o);

	/* Correct total collectable size, so GC will not be too aggressive. */
	size = seal_sizeof(o);
	uj_mem_dec_total(MEM(L), size);
	G(L)->gc.sealed += size;

	seal_traverse(L, o, seal_commit);
}

static void unseal_obj(global_State *g, GCobj *o)
{
	const size_t size = seal_sizeof(o);

	lua_assert(uj_obj_is_sealed(o));
	seal_clear_mark(o);
	uj_mem_inc_total(MEM_G(g), size);
	g->gc.sealed -= size;
}

static void unseal_chain(global_State *g, GCobj **p)
{
	GCobj *o;
	while (NULL != (o = *p)) {
		if (uj_obj_is_sealed(o))
			unseal_obj(g, o);
		p = &o->gch.nextgc;
	}
}

static void unseal_root(global_State *g)
{
	unseal_chain(g, &g->gc.root);
}

static void unseal_strhash(global_State *g)
{
	size_t i;
	uj_strhash_t *strhash;
	if (gl_datastate(g)) /* Not ours to manage. */
		return;
	strhash = gl_strhash_sealed(g);
	for (i = 0; i <= strhash->mask; i++)
		unseal_chain(g, &strhash->hash[i]);
}

/* Public API */

void uj_obj_seal(lua_State *L, GCobj *o)
{
	if (uj_obj_is_sealed(o))
		return;

	uj_obj_pmark(L, o, seal_mark, seal_unmark);

	seal_commit(L, o);
	seal_relink(L);
}

void uj_obj_unseal_all(global_State *g)
{
	unseal_root(g);
	unseal_strhash(g);
	lua_assert(g->gc.sealed == 0);
}
