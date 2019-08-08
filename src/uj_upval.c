/*
 * Upvalue handling.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "lj_obj.h"
#include "lj_gc.h"
#include "uj_mem.h"

#ifdef NDEBUG
static void upval_assert_list_integrity(const GCupval *uv)
{
	UNUSED(uv);
}
#else
static void upval_assert_list_integrity(const GCupval *uv)
{
	lua_assert(uvprev(uvnext(uv)) == uv && uvnext(uvprev(uv)) == uv);
}
#endif /* !NDEBUG */

static void upval_link(GCupval *uv, global_State *g, GCobj **iter)
{
	lua_assert(0 == uv->closed);

	uv->nextgc = *iter; /* Insert into sorted list of open upvalues. */
	*iter = obj2gco(uv);
	uv->prev = &g->uvhead; /* Insert into GC list, too. */
	uv->next = g->uvhead.next;
	uvnext(uv)->prev = uv;
	g->uvhead.next = uv;
}

static void upval_unlink(GCupval *uv)
{
	upval_assert_list_integrity(uv);
	uvnext(uv)->prev = uv->prev;
	uvprev(uv)->next = uv->next;
}

/* Create a new open upvalue pointing to slot. */
static GCupval *upval_new_open(lua_State *L, TValue *slot)
{
	GCupval *uv = (GCupval *)uj_mem_alloc(L, sizeof(GCupval));
	newwhite(G(L), uv);
	uv->gct = ~LJ_TUPVAL;
	uv->closed = 0; /* Still open. */
	uv->v = slot; /* Pointing to the stack slot. */
	return uv;
}

/*
 * Create a new open upvalue pointing to slot and link it
 * to all relevant lists.
 */
static GCupval *upval_new_open_linked(lua_State *L, TValue *slot, GCobj **iter)
{
	GCupval *uv = upval_new_open(L, slot);
	/* NOBARRIER: The GCupval is new (marked white) and open. */
	upval_link(uv, G(L), iter);
	upval_assert_list_integrity(uv);
	return uv;
}

GCupval *uj_upval_new_closed_empty(lua_State *L)
{
	GCupval *uv = (GCupval *)uj_obj_new(L, sizeof(GCupval));
	uv->gct = ~LJ_TUPVAL;
	uv->closed = 1;
	setnilV(&uv->tv);
	uv->v = &uv->tv;
	return uv;
}

GCupval *uj_upval_find(lua_State *L, TValue *slot)
{
	GCobj **iter = &L->openupval;
	GCupval *uv;

	/*
	 * Search the sorted list of open upvalues.
	 * If found open upvalue pointing to same slot, return it (resurrecting,
	 * if it's dead). If no matching upvalue found, create a new one.
	 */

	while (*iter != NULL && uvval((uv = gco2uv(*iter))) >= slot) {
		lua_assert(!uv->closed && uvval(uv) != &uv->tv);

		if (uvval(uv) != slot) {
			iter = &uv->nextgc;
			continue;
		}

		if (isdead(G(L), obj2gco(uv)))
			flipwhite(obj2gco(uv));
		return uv;
	}

	return upval_new_open_linked(L, slot, iter);
}

void uj_upval_free(global_State *g, GCupval *uv)
{
	if (!uv->closed)
		upval_unlink(uv);
	uj_mem_free(MEM_G(g), uv, sizeof(*uv));
}

void uj_upval_close(lua_State *L, TValue *level)
{
	GCupval *uv;
	global_State *g = G(L);
	while (L->openupval != NULL &&
	       uvval((uv = gco2uv(L->openupval))) >= level) {
		GCobj *o = obj2gco(uv);
		lua_assert(!isblack(o) && !uv->closed && uvval(uv) != &uv->tv);
		L->openupval = uv->nextgc; /* No longer in open list. */
		if (isdead(g, o)) {
			uj_upval_free(g, uv);
		} else {
			upval_unlink(uv);
			lj_gc_closeuv(g, uv);
		}
	}
}
