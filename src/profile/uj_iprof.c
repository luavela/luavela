/*
 * uJIT instrumenting profiler
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "lj_obj.h"
#include "lj_tab.h"
#include "uj_lib.h"
#include "uj_mem.h"
#include "uj_str.h"
#include "profile/uj_iprof_iface.h"

#ifdef UJIT_IPROF_ENABLED

#define IPROF_CAPACITY (1 << 14)

static LJ_AINLINE int iprof_mcmp(enum iprof_node_type a, enum iprof_node_type b)
{
	return a == b ? 0 : a < b ? -1 : 1;
}

static LJ_AINLINE int iprof_time(struct iprof_node *node)
{
	return clock_gettime(CLOCK_MONOTONIC, &node->time);
}

static struct iprof_node *iprof_talloc(lua_State *L)
{
	struct iprof *iprof = &L->iprof;

	iprof->tape = uj_mem_alloc_nothrow(MEM(L), IPROF_CAPACITY);
	if (!iprof->tape)
		return NULL;
	iprof->capacity = IPROF_CAPACITY / sizeof(*(iprof->tape));
	memset(iprof->tape, 0, IPROF_CAPACITY);
	return iprof->tape;
}

void uj_iprof_tfree(lua_State *L)
{
	struct iprof *iprof = &L->iprof;

	if (!iprof->capacity)
		return;
	uj_mem_free(MEM(L), iprof->tape, IPROF_CAPACITY);
	iprof->size = iprof->capacity = 0;
}

static struct iprof_node *iprof_tick(struct iprof *iprof,
				     enum iprof_node_type type)
{
	struct iprof_node *node = &iprof->tape[iprof->size];

	if (iprof->size == iprof->capacity)
		return NULL;

	node->type = type;
	node->mode = iprof->mode;
	iprof_time(node);

	iprof->size++;

	return node;
}

enum iprof_status uj_iprof_start(struct lua_State *L, const char *name,
				 enum iprof_mode mode)
{
	struct iprof *iprof = &L->iprof;
	struct iprof_node *node;

	if (!iprof->profiling && !iprof_talloc(L))
		return IPROF_ERRMEM;

	iprof->mode = mode;

	node = iprof_tick(iprof, IPROF_START);
	if (!node)
		return IPROF_ERRMEM;

	node->name = name;
	iprof->profiling++;

	return IPROF_SUCCESS;
}

enum iprof_status uj_iprof_stop(struct lua_State *L, struct GCtab **result)
{
	struct iprof *iprof = &L->iprof;
	GCtab *entity;
	struct iprof_node *node;
	struct iprof_node *const stop = iprof_tick(iprof, IPROF_STOP);
	double lua_c = .0f;
	double wall_c = .0f;

	if (!stop)
		return IPROF_ERRMEM;

	iprof->profiling--;

	*result = lj_tab_new(L, 0, 1);
	entity = lj_tab_new(L, 0, 1);

	for (node = stop; node >= iprof->tape; node--) {
		double tick = (node->time.tv_sec + 1e-9 * node->time.tv_nsec) *
			      iprof_mcmp(node->type, IPROF_DUMMY);
		switch (node->type) {
		case IPROF_START:
		case IPROF_STOP:
			wall_c += tick;
			/* Fallthrough */
		case IPROF_RESUME:
		case IPROF_YIELD:
			lua_c += tick;
			/* Fallthrough */
		case IPROF_DUMMY:
			break;
		}
		if (node->type == IPROF_START)
			break;
	}

	/* NOBARRIER: The table is new (marked white). */
	settabV(L, lj_tab_setstr(L, *result, uj_str_newz(L, node->name)),
		entity);
	node->type = IPROF_DUMMY;
	for (iprof->mode = node[-1].mode; node <= stop; node++)
		node->mode = iprof->mode;
	stop->type = IPROF_DUMMY;
	/* NOBARRIER: The table is new (marked white). */
	setnumV(lj_tab_setstr(L, entity, uj_str_newz(L, "wall")), wall_c);
	setnumV(lj_tab_setstr(L, entity, uj_str_newz(L, "lua")), lua_c);

	if (iprof->profiling)
		return IPROF_SUCCESS;
	uj_iprof_tfree(L);
	return IPROF_SUCCESS;
}

void uj_iprof_tick(struct lua_State *L, enum iprof_node_type type)
{
	struct iprof_node *node = iprof_tick(&L->iprof, type);

	if (!node)
		return;

	node->function = curr_func(L);
}

#else /* !UJIT_IPROF_ENABLED */

enum iprof_status uj_iprof_start(struct lua_State *L, const char *name,
				 enum iprof_mode mode)
{
	UNUSED(L);
	UNUSED(mode);
	UNUSED(name);

	return IPROF_ERRNYI;
}

enum iprof_status uj_iprof_stop(struct lua_State *L, struct GCtab **result)
{
	UNUSED(L);
	UNUSED(result);

	return IPROF_ERRNYI;
}

#endif /* UJIT_IPROF_ENABLED */
