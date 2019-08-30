/*
 * uJIT instrumenting profiler
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "lj_obj.h"
#include "lj_tab.h"
#include "profile/uj_iprof_iface.h"

#ifdef UJIT_IPROF_ENABLED

#include <time.h>
#include "lj_gc.h"
#include "uj_mem.h"
#include "uj_str.h"
#include "uj_sbuf.h"
#include "uj_state.h"
#include "uj_throw.h"

struct iprof_callers {
	GCtab *stack;
	size_t depth;
};

struct LJ_ALIGN(8) iprof {
	const char *name; /* profiling entity name */
	ptrdiff_t base; /* ujit.iprof.start caller's L->base */
	struct iprof *pframe; /* parent frame */
	struct iprof_callers callers; /* stack of callers */
	GCtab *current; /* current entity being profiled */
	enum iprof_mode mode; /* profiling mode */
	double discrepancy; /* time spent on collecting stats */
	unsigned clevel; /* current level of call frames within iprof frame */
	unsigned climit; /* maximum level of call frames within iprof frame */
};

static LJ_AINLINE double iprof_abstime(double tick, enum iprof_ev_type type)
{
	lua_assert(type != IPROF_DUMMY);
	return type < IPROF_DUMMY ? -tick : tick;
}

static LJ_AINLINE double iprof_gettime(lua_State *L, enum iprof_ev_type type)
{
	struct timespec time;
#ifdef CLOCK_MONOTONIC
	const int clock = CLOCK_MONOTONIC;
#else /* !CLOCK_MONOTONIC */
	const int clock = CLOCK_REALTIME;
#endif /* CLOCK_MONOTONIC */

	if (LJ_UNLIKELY(clock_gettime(clock, &time)))
		uj_throw(L, LUA_ERRRUN);

	/* Return value considering the event type */
	return iprof_abstime(time.tv_sec + 1e-9 * time.tv_nsec, type);
}

/* Keys {{{ */

void uj_iprof_keys(lua_State *L)
{
	/* FIXME: Find the better way to initialize this array */
	G(L)->iprof_keys[IPROF_KEY_LUA] = uj_str_newz(L, "lua");
	G(L)->iprof_keys[IPROF_KEY_WALL] = uj_str_newz(L, "wall");
	G(L)->iprof_keys[IPROF_KEY_SUBS] = uj_str_newz(L, "subs");
	G(L)->iprof_keys[IPROF_KEY_CALLS] = uj_str_newz(L, "calls");

	for (size_t i = 0; i < IPROF_KEY_MAX; i++)
		fixstring(G(L)->iprof_keys[i]);
}

void uj_iprof_unkeys(lua_State *L)
{
	for (size_t i = 0; i < IPROF_KEY_MAX; i++)
		unfixstring(G(L)->iprof_keys[i]);
}

static LJ_AINLINE GCstr *iprof_key(lua_State *L, enum iprof_keys key)
{
	return G(L)->iprof_keys[key];
}

/* }}} */

/* Entity {{{ */

static GCtab *iprof_entity_new(lua_State *L)
{
	/* Don't need array part and need moderately-sized hash part */
	GCtab *entity = lj_tab_new(L, 0, 2);

	setnumV(lj_tab_setstr(L, entity, iprof_key(L, IPROF_KEY_CALLS)), 1);
	setnumV(lj_tab_setstr(L, entity, iprof_key(L, IPROF_KEY_WALL)), 0);
	setnumV(lj_tab_setstr(L, entity, iprof_key(L, IPROF_KEY_LUA)), 0);

	return entity;
}

static LJ_AINLINE void iprof_entity_acc(lua_State *L, GCtab *entity,
					enum iprof_keys key, double value)
{
	GCstr *strkey = iprof_key(L, key);

	setnumV(lj_tab_setstr(L, entity, strkey),
		numV(lj_tab_getstr(entity, strkey)) + value);
}

static const char *iprof_entity_name(lua_State *L, GCfunc *function)
{
	char what;
	const char *source;
	BCLine linedefined = -1;
	struct sbuf *nbuf = uj_sbuf_reset_tmp(L);

	if (isluafunc(function)) {
		GCproto *pt = funcproto(function);

		source = proto_chunknamestr(pt);
		linedefined = pt->firstline;
		what = (pt->firstline || !pt->numline) ? 'L' : 'm';
	} else {
		source = "=[C]";
		what = 'C';
	}

	if (isffunc(function)) {
		uj_sbuf_push_cstr(nbuf, "builtin #");
		uj_sbuf_push_num(nbuf, function->c.ffid);
	} else if (what == 'm') {
		uj_sbuf_push_cstr(nbuf, "main chunk");
	} else if (what == 'C') {
		uj_sbuf_push_char(nbuf, '@');
		UJ_PEDANTIC_OFF
		/* casting a function ptr to void* */
		uj_sbuf_push_ptr(nbuf, (void *)function->c.f);
		UJ_PEDANTIC_ON
	} else {
		uj_sbuf_push_cstr(nbuf, "function ");
		uj_sbuf_push_cstr(nbuf, source);
		uj_sbuf_push_char(nbuf, ':');
		uj_sbuf_push_num(nbuf, linedefined);
	}

	uj_sbuf_push_char(nbuf, 0);

	return uj_sbuf_front(nbuf);
}

static void iprof_entity_finalize(lua_State *L, GCtab *entity)
{
	GCtab *subs, *fsubs;
	const TValue *test =
		lj_tab_getstr(entity, iprof_key(L, IPROF_KEY_SUBS));

	if (!test)
		return;

	fsubs = tabV(test);
	/*
	 * FIXME: Considering possible table resize iteration with replacing
	 * existing function keys with corresponding names is not stable
	 * Is there a better solution for this? I hope to find it later...
	 */
	subs = lj_tab_new(L, 0, hsize2hbits(fsubs->hmask + 1));
	settabV(L, lj_tab_setstr(L, entity, iprof_key(L, IPROF_KEY_SUBS)),
		subs);
	lj_gc_anybarriert(L, entity);

	for (uint64_t iter = 0; (iter = lj_tab_iterate(L, fsubs, iter));
	     L->top -= 2) {
		GCstr *name =
			uj_str_newz(L, iprof_entity_name(L, funcV(L->top - 2)));
		GCtab *sub = tabV(L->top - 1);

		iprof_entity_finalize(L, sub);
		if (L->iprof->mode == IPROF_INCLUSIVE) {
			iprof_entity_acc(
				L, entity, IPROF_KEY_WALL,
				numV(lj_tab_getstr(
					sub, iprof_key(L, IPROF_KEY_WALL))));
			iprof_entity_acc(
				L, entity, IPROF_KEY_LUA,
				numV(lj_tab_getstr(
					sub, iprof_key(L, IPROF_KEY_LUA))));
		}
		settabV(L, lj_tab_setstr(L, subs, name), sub);
	}
}

/* }}} */

/* Callers {{{ */

static LJ_AINLINE void
iprof_callers_init(lua_State *L, struct iprof_callers *callers, unsigned climit)
{
	callers->depth = 0;

	/* Don't need hash part and need moderately-sized array part */
	callers->stack = lj_tab_new(L, climit, 0);
}

static GCtab *iprof_callers_push(lua_State *L)
{
	struct iprof *iprof = L->iprof;
	struct iprof_callers *callers = &iprof->callers;
	GCtab **current = &iprof->current;
	const TValue *test;
	TValue fkey;

	settabV(L, lj_tab_setint(L, callers->stack, callers->depth), *current);
	callers->depth++;

	setfuncV(L, &fkey, curr_func(L));

	test = lj_tab_getstr(*current, iprof_key(L, IPROF_KEY_SUBS));

	if (test) {
		GCtab *fsubs = tabV(test);

		test = lj_tab_get(L, fsubs, &fkey);
		if (tvisnil(test)) {
			*current = iprof_entity_new(L);
			settabV(L, lj_tab_set(L, fsubs, &fkey), *current);
			lj_gc_anybarriert(L, fsubs);
		} else {
			*current = tabV(test);
			iprof_entity_acc(L, *current, IPROF_KEY_CALLS, 1);
		}
	} else {
		GCtab *fsubs = lj_tab_new(L, 0, 1);

		settabV(L,
			lj_tab_setstr(L, *current,
				      iprof_key(L, IPROF_KEY_SUBS)),
			fsubs);
		lj_gc_anybarriert(L, *current);
		*current = iprof_entity_new(L);
		/* NOBARRIER: The table is new (marked white). */
		settabV(L, lj_tab_set(L, fsubs, &fkey), *current);
	}

	return iprof->current;
}

static GCtab *iprof_callers_pop(lua_State *L)
{
	struct iprof *iprof = L->iprof;
	struct iprof_callers *callers = &iprof->callers;

	lua_assert(callers->depth > 0 && callers->depth < LJ_MAX_XLEVEL);

	callers->depth--;
	iprof->current = tabV(lj_tab_getint(callers->stack, callers->depth));

	return iprof->current;
}

/* }}} */

/* Frames {{{ */

static LJ_AINLINE unsigned iprof_climit(struct iprof *iprof, unsigned climit)
{
	unsigned pclimit = iprof ? iprof->climit - iprof->clevel : 0;

	return climit < pclimit ? pclimit : climit;
}

static void iprof_push(lua_State *L, const char *name, enum iprof_mode mode,
		       unsigned climit)
{
	struct iprof *iprof = uj_mem_alloc(L, sizeof(*iprof));
	GCtab *anchor_v = lj_tab_new(L, 0, 1);
	TValue anchor_k;

	iprof->pframe = L->iprof;

	iprof->name = name;
	iprof->mode = mode;
	iprof->clevel = 0;
	iprof->discrepancy = 0;
	iprof->climit = iprof_climit(iprof->pframe, climit);
	iprof->base = uj_state_stack_save(L, L->base);
	iprof->current = iprof_entity_new(L);
	iprof_callers_init(L, &iprof->callers, climit);

	setthreadV(L, &anchor_k, L);
	/* NOBARRIER: The table is new (marked white). */
	settabV(L, lj_tab_setstr(L, anchor_v, uj_str_newz(L, "stack")),
		iprof->callers.stack);
	settabV(L, lj_tab_setstr(L, anchor_v, uj_str_newz(L, "entity")),
		iprof->current);

	settabV(L, lj_tab_set(L, tabV(registry(L)), &anchor_k), anchor_v);
	lj_gc_anybarriert(L, tabV(registry(L)));

	L->iprof = iprof;
}

static void iprof_pop(lua_State *L)
{
	struct iprof *iprof = L->iprof;
	TValue anchor_k;

	L->iprof = iprof->pframe;

	setthreadV(L, &anchor_k, L);
	setnilV(lj_tab_set(L, tabV(registry(L)), &anchor_k));

	/* TODO: (Nested profiling) store existing results for parent frame */

	uj_mem_free(MEM(L), iprof, sizeof(*iprof));
}

/* }}} */

void uj_iprof_release(lua_State *L)
{
	while (L->iprof)
		iprof_pop(L);
}

void uj_iprof_unwind(lua_State *L)
{
	struct iprof *iprof = L->iprof;

	if (!iprof)
		return;

	if (uj_state_stack_restore(L, iprof->base) <= L->base)
		uj_iprof_tick(L, IPROF_RETURN);
	else
		iprof_pop(L);
}

static LJ_AINLINE void iprof_clevel_update(struct iprof *iprof,
					   enum iprof_ev_type type)
{
	switch (type) {
	case IPROF_STOP:
	case IPROF_RETURN:
		iprof->clevel--;
		break;
	case IPROF_START:
	case IPROF_CALL:
		iprof->clevel++;
		break;
	case IPROF_CALLT:
		/* implicit atomic iprof->clevel-- and iprof->clevel++ */
	case IPROF_YIELD:
	case IPROF_RESUME:
		break;
	case IPROF_DUMMY:
	default:
		lua_assert(0);
	}
}

static LJ_AINLINE int iprof_overclimit(struct iprof *iprof,
				       enum iprof_ev_type type)
{
	switch (type) {
	case IPROF_CALL:
	case IPROF_CALLT:
		return iprof->climit < iprof->clevel;
	case IPROF_RETURN:
		return iprof->climit < iprof->clevel + 1;
	case IPROF_START:
	case IPROF_STOP:
	case IPROF_YIELD:
	case IPROF_RESUME:
		return 0;
	case IPROF_DUMMY:
	default:
		lua_assert(0);
		/* Unreachable */
		return 0;
	}
}

void uj_iprof_tick(lua_State *L, enum iprof_ev_type type)
{
	struct iprof *iprof = L->iprof;
	double enter, leave;
	double tick = enter = iprof_gettime(L, type);
	GCtab *entity = iprof->current;

	iprof_clevel_update(iprof, type);

	if (iprof_overclimit(iprof, type))
		return;

	tick -= iprof_abstime(iprof->discrepancy, type);

	switch (type) {
	case IPROF_STOP:
	case IPROF_RETURN:
	case IPROF_CALLT:
		iprof_entity_acc(L, entity, IPROF_KEY_WALL, tick);
		iprof_entity_acc(L, entity, IPROF_KEY_LUA, tick);

		if (iprof->mode == IPROF_PLAIN)
			break;

		entity = iprof_callers_pop(L);

		if (type == IPROF_STOP)
			break;

		iprof_entity_acc(L, entity, IPROF_KEY_WALL, -tick);
		iprof_entity_acc(L, entity, IPROF_KEY_LUA, -tick);

		if (type == IPROF_RETURN)
			break;

		/*
		 * NB! This is fragile. IPROF_CALLT is related to both "opening"
		 * and * "closing" node types however formally it's related only
		 * to * the latter one. Thus its tick value have to be negative
		 * within the following flow
		 */
		tick = -tick;

		/* Fallthrough for IPROF_CALLT */
	case IPROF_START:
	case IPROF_CALL:
		iprof_entity_acc(L, entity, IPROF_KEY_WALL,
				 type == IPROF_START ?
					 tick * (iprof->mode == IPROF_PLAIN) :
					 -tick);
		iprof_entity_acc(L, entity, IPROF_KEY_LUA,
				 type == IPROF_START ?
					 tick * (iprof->mode == IPROF_PLAIN) :
					 -tick);

		if (iprof->mode == IPROF_PLAIN)
			break;

		entity = iprof_callers_push(L);

		iprof_entity_acc(L, entity, IPROF_KEY_WALL, tick);
		iprof_entity_acc(L, entity, IPROF_KEY_LUA, tick);
		break;
	case IPROF_RESUME:
	case IPROF_YIELD:
		iprof_entity_acc(L, entity, IPROF_KEY_LUA, tick);
		break;
	case IPROF_DUMMY:
	default:
		lua_assert(0);
	}

	leave = iprof_gettime(L, type);
	iprof->discrepancy += iprof_abstime(leave - enter, type);

	lua_assert(iprof->discrepancy > 0);
}

enum iprof_status uj_iprof_start(lua_State *L, const char *name,
				 enum iprof_mode mode, unsigned climit)
{
	if (L->iprof)
		return IPROF_ERRERR;

	iprof_push(L, name, mode, climit);

	uj_iprof_tick(L, IPROF_START);

	return IPROF_SUCCESS;
}

enum iprof_status uj_iprof_stop(lua_State *L, GCtab **result)
{
	struct iprof *iprof = L->iprof;

	if (!iprof)
		return IPROF_ERRERR;

	uj_iprof_tick(L, IPROF_STOP);

	lua_assert(!iprof->clevel);

	iprof_entity_finalize(L, iprof->current);

	*result = lj_tab_new(L, 0, 1);

	/* NOBARRIER: The table is new (marked white). */
	settabV(L, lj_tab_setstr(L, *result, uj_str_newz(L, iprof->name)),
		iprof->current);

	iprof_pop(L);

	lua_assert(!L->iprof);

	return IPROF_SUCCESS;
}

#else /* !UJIT_IPROF_ENABLED */

enum iprof_status uj_iprof_start(lua_State *L, const char *name,
				 enum iprof_mode mode, unsigned climit)
{
	UNUSED(L);
	UNUSED(name);
	UNUSED(mode);
	UNUSED(climit);

	return IPROF_ERRNYI;
}

enum iprof_status uj_iprof_stop(lua_State *L, struct GCtab **result)
{
	UNUSED(L);
	UNUSED(result);

	return IPROF_ERRNYI;
}

#endif /* UJIT_IPROF_ENABLED */
