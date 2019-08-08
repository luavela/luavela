/*
 * State and stack handling.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _UJ_STATE_H
#define _UJ_STATE_H

#include "lj_obj.h"

struct timeval;

void uj_state_stack_grow1(lua_State *L);
void uj_state_stack_relimit(lua_State *L);
void uj_state_stack_shrink(lua_State *L, size_t used);
void uj_state_stack_grow(lua_State *L, size_t need);

static void LJ_AINLINE uj_state_stack_incr_top(lua_State *L)
{
	L->top++;
	if (L->top >= L->maxstack)
		uj_state_stack_grow1(L);
}

static LJ_AINLINE ptrdiff_t uj_state_stack_save(const lua_State *L,
						const TValue *p)
{
	return (char *)p - (char *)L->stack;
}

static LJ_AINLINE TValue *uj_state_stack_restore(const lua_State *L,
						 ptrdiff_t n)
{
	return (TValue *)((char *)L->stack + n);
}

static LJ_AINLINE void uj_state_stack_check(lua_State *L, size_t need)
{
	if (((char *)L->maxstack - (char *)L->top) <=
	    (ptrdiff_t)need * (ptrdiff_t)sizeof(TValue))
		uj_state_stack_grow(L, need);
}

/*
 * Quirk for syncing L's top with the actual stack top, which is needed when
 * the control is transferred from VM to the rest of the core platform.
 */
static LJ_AINLINE void uj_state_stack_sync_top(lua_State *L)
{
	if (curr_funcisL(L))
		L->top = curr_topL(L);
}

lua_State *uj_state_new(lua_State *L);
void uj_state_free(global_State *g, lua_State *L);

/* Creates a new VM instance configuration specified in opt. */
lua_State *uj_state_newstate(const struct luae_Options *opt);
void uj_state_close(lua_State *L);

/*
 * For the given coroutine, calculate the absolute number of ticks
 * before coroutine's timeout.
 */
void uj_state_setexpticks(lua_State *L);

lua_CFunction uj_state_settimeout_callback(lua_State *L,
					   lua_CFunction callback);
int uj_state_settimeout(lua_State *L, const struct timeval *timeout,
			int restart);

static LJ_AINLINE int uj_state_has_timeout(const lua_State *L)
{
	return L->timeout.usec > 0;
}

/*
 * External events that the platform runs in the context of the
 * given coroutine. Those events can either fully interrupt coroutine's normal
 * execution (like executing an error function) or run in the interleaved
 * manner (like trace recording performed by the JIT compiler). The term
 * "external" should be understood as "not invoked by the coroutine's code
 * directly".
 */

typedef uint16_t ext_ev_t;

#define EXTEV_ERRRUN_FUNC ((ext_ev_t)0x0001) /* Running an error function */
#define EXTEV_TIMEOUT_FUNC ((ext_ev_t)0x0002) /* Running a timeout function */
#define EXTEV_JIT_ACTIVE ((ext_ev_t)0x0004) /* JIT compiler is active */
#define EXTEV_GC_FINALIZER ((ext_ev_t)0x0008) /* Running a __gc finalizer */
#define EXTEV_VM_INIT ((ext_ev_t)0x0010) /* VM is being initialized */
#define EXTEV_ANY_EVENT ((ext_ev_t)0xffff)

static LJ_AINLINE int uj_state_has_event(const lua_State *L, ext_ev_t ev)
{
	return L->events & ev;
}

static LJ_AINLINE void uj_state_add_event(lua_State *L, ext_ev_t ev)
{
	lua_assert(!uj_state_has_event(L, ev));
	L->events |= ev;
}

static LJ_AINLINE void uj_state_remove_event(lua_State *L, ext_ev_t ev)
{
	lua_assert(uj_state_has_event(L, ev));
	L->events &= ~ev;
}

#endif /* !_UJ_STATE_H */
