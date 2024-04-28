/*
 * State and stack handling.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 *
 * Portions taken verbatim or adapted from the Lua interpreter.
 * Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
 */

#include "lj_obj.h"
#include "lj_gc.h"
#include "uj_obj_seal.h"
#include "uj_obj_immutable.h"
#include "uj_mem.h"
#include "uj_throw.h"
#include "uj_err.h"
#include "uj_dispatch.h"
#include "uj_errmsg.h"
#include "uj_sbuf.h"
#include "uj_state.h"
#include "lj_tab.h"
#include "uj_upval.h"
#include "uj_meta.h"
#include "lj_frame.h"
#include "uj_timerint.h"
#if LJ_HASFFI
#include "ffi/lj_ctype.h"
#endif
#include "frontend/lj_lex.h"
#include "utils/random.h"
#include "utils/strhash.h"
#include "uj_strhash.h"
#include "profile/uj_profile_iface.h"
#include "profile/uj_memprof_iface.h"
#include "profile/uj_iprof_iface.h"
#include "dump/uj_dump_iface.h"
#ifdef UJIT_COVERAGE
#include "uj_coverage.h"
#endif /* UJIT_COVERAGE */

/* -- Global initialization ----------------------------------------------- */

static uint8_t uj_global_init_done;

#ifdef UJIT_IS_THREAD_SAFE

#include <pthread.h>

pthread_mutex_t uj_global_init_mutex = PTHREAD_MUTEX_INITIALIZER;

#define LOCK_GLOBAL_INIT() (pthread_mutex_lock(&uj_global_init_mutex))
#define UNLOCK_GLOBAL_INIT() (pthread_mutex_unlock(&uj_global_init_mutex))

#else

#define LOCK_GLOBAL_INIT()
#define UNLOCK_GLOBAL_INIT()

#endif /* UJIT_IS_THREAD_SAFE */

/*
 * Payload code for global initialization.
 * All VM-independent data/code has to be initialized/executed here.
 */
static void state_uj_global_init(void)
{
	if (uj_global_init_done)
		return;

	srandom(random_time_seed());

	uj_global_init_done = 1;
}

static void uj_global_init(void)
{
	LOCK_GLOBAL_INIT();
	state_uj_global_init();
	UNLOCK_GLOBAL_INIT();
}

/* -- Stack handling ------------------------------------------------------ */

/* Stack sizes. */
#define LJ_STACK_MIN LUA_MINSTACK /* Min stack size. */
#define LJ_STACK_MAX 65500 /* Max stack size for a coroutine (<64K). */
#define LJ_STACK_START (2 * LJ_STACK_MIN) /* Starting stack size. */
#define LJ_STACK_MAXEX (LJ_STACK_MAX + 1 + LJ_STACK_EXTRA)

/*
 * Explanation of LJ_STACK_EXTRA:
 *
 * Calls to metamethods store their arguments beyond the current top
 * without checking for the stack limit. This avoids stack resizes which
 * would invalidate passed TValue pointers. The stack check is performed
 * later by the function header. This can safely resize the stack or raise
 * an error. Thus we need some extra slots beyond the current stack limit.
 *
 * Most metamethods need 4 slots above top (cont, mobj, arg1, arg2) plus
 * one extra slot if mobj is not a function. Only uj_meta_tset needs 5
 * slots above top, but then mobj is always a function. So we can get by
 * with 5 extra slots.
 */

static LJ_AINLINE void state_stack_free(global_State *g, lua_State *L)
{
	uj_mem_free(MEM_G(g), L->stack, L->stacksize * sizeof(TValue));
}

static int state_stack_is_profiled(const lua_State *L)
{
	const TValue *guesttop = G(L)->top_frame.guesttop.interp_base;
	const TValue *st = L->stack;
	const size_t sz = L->stacksize;

	return ((size_t)(guesttop - st) <= sz);
}

/* Resize stack slots and adjust pointers in state. */
static void state_resizestack(lua_State *L, size_t n)
{
	TValue *st, *oldst = L->stack;
	ptrdiff_t delta;
	size_t oldsize = L->stacksize;
	size_t realsize = n + 1 + LJ_STACK_EXTRA;
	GCobj *up;
	struct vmstate_context vmsc;
	/*
	 * state_stack_is_profiled() should be run strictly before stack
	 * relocation
	 */
	const int update_guesttop = state_stack_is_profiled(L);

	uj_vmstate_save(G(L)->vmstate, &vmsc);
	uj_vmstate_set(&G(L)->vmstate, UJ_VMST_INTERP);

	lua_assert((size_t)(L->maxstack - oldst) ==
		   L->stacksize - LJ_STACK_EXTRA - 1);
	st = (TValue *)uj_mem_realloc(L, L->stack,
				      (L->stacksize * sizeof(TValue)),
				      (realsize * sizeof(TValue)));
	L->stack = st;
	delta = (char *)st - (char *)oldst;
	L->maxstack = st + n;
	while (oldsize < realsize) /* Clear new slots. */
		setnilV(st + oldsize++);
	L->stacksize = realsize;
	L->base = (TValue *)((char *)L->base + delta);
	L->top = (TValue *)((char *)L->top + delta);
	for (up = L->openupval; up != NULL; up = gcnext(up))
		gco2uv(up)->v = (TValue *)((char *)uvval(gco2uv(up)) + delta);
	if (L == G(L)->jit_L)
		G(L)->jit_base = (TValue *)((char *)G(L)->jit_base + delta);

	/*
	 * Guesttop will be updated only if it belongs to stack being
	 * resized
	 */
	if (update_guesttop)
		G(L)->top_frame.guesttop.interp_base = L->base;

	uj_vmstate_restore(&G(L)->vmstate, &vmsc);
}

/* Relimit stack after error, in case the limit was overdrawn. */
void uj_state_stack_relimit(lua_State *L)
{
	if (L->stacksize > LJ_STACK_MAXEX &&
	    L->top - L->stack < LJ_STACK_MAX - 1)
		state_resizestack(L, LJ_STACK_MAX);
}

/* Try to shrink the stack (called from GC). */
void uj_state_stack_shrink(lua_State *L, size_t used)
{
	/* Avoid stack shrinking while handling stack overflow. */
	if (L->stacksize > LJ_STACK_MAXEX)
		return;
	if (4 * used < L->stacksize &&
	    2 * (LJ_STACK_START + LJ_STACK_EXTRA) < L->stacksize &&
	    L != G(L)->jit_L) /* Don't shrink stack of live trace. */
		state_resizestack(L, L->stacksize >> 1);
}

/* Try to grow stack. */
void uj_state_stack_grow(lua_State *L, size_t need)
{
	size_t n;

	/* Overflow while handling overflow? */
	if (L->stacksize > LJ_STACK_MAXEX)
		uj_throw(L, LUA_ERRERR);
	n = L->stacksize + need;
	if (n > LJ_STACK_MAX) {
		n += 2 * LUA_MINSTACK;
	} else if (n < 2 * L->stacksize) {
		n = 2 * L->stacksize;
		if (n >= LJ_STACK_MAX)
			n = LJ_STACK_MAX;
	}
	state_resizestack(L, n);
	if (L->stacksize > LJ_STACK_MAXEX)
		uj_err(L, UJ_ERR_STKOV);
}

void uj_state_stack_grow1(lua_State *L)
{
	uj_state_stack_grow(L, 1);
}

/* Allocate basic stack for new state. */
static void state_stack_init(lua_State *L1, lua_State *L)
{
	const size_t size = (const size_t)(LJ_STACK_START + LJ_STACK_EXTRA);
	TValue *stend;
	TValue *st = (TValue *)uj_mem_alloc(L, size * sizeof(TValue));

	L1->stack = st;
	L1->stacksize = size;
	stend = st + L1->stacksize;
	L1->maxstack = stend - LJ_STACK_EXTRA - 1;
	L1->base = L1->top = st + 1;
	/* Needed for curr_funcisL() on empty stack */
	frame_set_bottom(L1, st);
	st++;
	while (st < stend) /* Clear new slots. */
		setnilV(st++);
}

/* -- State handling ------------------------------------------------------ */

/* Open parts that may cause memory-allocation errors. */
static TValue *state_cpluaopen(lua_State *L, lua_CFunction dummy, void *ud)
{
	global_State *g = G(L);
	UNUSED(dummy);
	UNUSED(ud);

	state_stack_init(L, L);
	uj_strhash_init(gl_strhash(g), L);
	if (gl_datastate(g)) {
		lua_assert(0 == gl_strhash(g)->count);
		/*
		 * Must be done before any string allocations
		 * in newly created state
		 */
		*gl_strhash_sealed(g) = *gl_strhash_sealed(G(g->datastate));
	} else {
		/* Initialize it right away, just in case. */
		uj_strhash_init(gl_strhash_sealed(g), L);
	}
	/* NOBARRIER: State initialization, all objects are white. */
	L->env = lj_tab_new(L, 0, LJ_MIN_GLOBAL);
	settabV(L, registry(L), lj_tab_new(L, 0, LJ_MIN_REGISTRY));
	uj_meta_init(L);
	lj_lex_init(L);
	/* Preallocate memory error msg. */
	fixstring(uj_errmsg_str(L, UJ_ERR_ERRMEM));
	g->gc.threshold = 4 * uj_mem_total(MEM(L));
#if LJ_HASJIT
	lj_trace_initstate(g);
#endif

#ifdef UJIT_IPROF_ENABLED
	uj_iprof_keys(L);
#endif /* UJIT_IPROF_ENABLED */

	return NULL;
}

static void state_close_state(lua_State *L)
{
	global_State *g = G(L);
	struct mem_manager *mem = MEM(L);

	uj_profile_stop_vm(g);
	uj_memprof_stop_vm(g);
#ifdef UJIT_IPROF_ENABLED
	uj_iprof_release(L);
	uj_iprof_unkeys(L);
#endif /* UJIT_IPROF_ENABLED */
#ifdef UJIT_COVERAGE
	uj_coverage_stop(L);
#endif /* UJIT_COVERAGE */
#if LJ_HASJIT
	uj_dump_stop(L);
#endif
	uj_timerint_terminate_default();
	uj_upval_close(L, L->stack);
	uj_sbuf_free(L, &g->tmpbuf);
	lj_gc_freeall(g);
	/* NOTE: Do not touch L below this line, it is GC'ed */
	lua_assert(NULL == g->gc.root);
#if LJ_HASJIT
	lj_trace_freestate(g);
#endif
#if LJ_HASFFI
	lj_ctype_freestate(g);
#endif
	uj_strhash_destroy(gl_strhash(g), g);
	if (!gl_datastate(g)) {
		/* Otherwise it's not ours to manage. */
		uj_strhash_destroy(gl_strhash_sealed(g), g);
	}
	lua_assert(uj_mem_total(mem) == sizeof(struct GG_State));
	uj_mem_terminate(mem);
}

static int state_panic(lua_State *L)
{
	const char *s = lua_tostring(L, -1);

	fputs("PANIC: unprotected error in call to Lua API (", stderr);
	fputs(s ? s : "?", stderr);
	fputc(')', stderr);
	fputc('\n', stderr);
	fflush(stderr);
	return 0;
}

/*
 * DataState is a supplementary data structure that contains data used by
 * different global_State instances. It itself is implemented in a form of
 * a global_State, in order to make integration more convenient. As of now,
 * the following pieces of data are imported from DataState to target
 * global_State:
 *   - global_State.dataroot
 *     this is a nested data structure, a table itself, which might contain
 *     strings and other tables, alongside with primitive types
 *   - global_State.strhash_sealed
 *     this must contain at least all strings present across the dataroot,
 *     except possibly empty string
 *   - &global_State.strempty_own
 *     as this string by no means is registered in the strhash or
 *     strhash_sealed,we must import it as well just in case it is present
 *     somewhere in the dataroot. It is sealed in the DataState in order not
 *     to confuse garbage collectors of dependent states.
 *
 * NOTE: dependent states do not own any of these! They must not modify, nor
 *       delete objects from these collections!
 * ALSO NOTE: if particular state depends on the DataState, it must not call
 *       sealing interfaces, since it do not own its strhash_sealed!
 * FINALLY NOTE: currently it is not supported for a state to be created as
 *       dependent on some DataState and to be used as a DataState itself.
 *       It is not certain that something blows up if this happens, but just
 *       don't do it.
 */
lua_State *uj_state_newstate(const struct luae_Options *opt)
{
	struct mem_manager mem;
	struct GG_State *GG;
	global_State *g;
	lua_State *L;
	uj_strhash_t *strhash;
	uj_strhash_t *strhash_sealed;
	int cpluaopen_status;
	lua_State *datastate = opt != NULL ? opt->datastate : NULL;
	enum luaext_HashF hashftype = opt != NULL ? opt->hashftype
						  : LUAE_HASHF_DEFAULT;

	uj_global_init();

	if (uj_mem_init(&mem, opt) != 0)
		return NULL;

	GG = uj_mem_alloc_nothrow(&mem, sizeof(struct GG_State));
	if (NULL == GG)
		return NULL;
	memset(GG, 0, sizeof(struct GG_State));

	L = &GG->L;
	g = &GG->g;

	if (datastate == NULL) {
		switch (hashftype) {
		case LUAE_HASHF_CITY: {
			g->hashf = strhash_city;
			break;
		}
		case LUAE_HASHF_MURMUR:
		/* ditto case LUAE_HASHF_DEFAULT: */
		default: {
			g->hashf = strhash_murmur3;
			break;
		}
		}
	} else {
		g->hashf = G(datastate)->hashf;
	}

	lua_assert(g->hashf != NULL);

	/* New VM is obviously idle initially: */
	uj_vmstate_set(&g->vmstate, UJ_VMST_IDLE);
	strhash = gl_strhash(g);
	strhash_sealed = gl_strhash_sealed(g);

	memcpy(MEM_G(g), &mem, sizeof(struct mem_manager));

	L->gct = ~LJ_TTHREAD;
	L->marked = LJ_GC_WHITE0 | LJ_GC_FIXED; /* Prevent free. */
	L->dummy_ffid = FF_C;
	memset(&L->timeout, 0, sizeof(struct coro_timeout));
	L->glref = g;
	g->gc.currentwhite = LJ_GC_WHITE0 | LJ_GC_FIXED;
	g->nullobj = NULL;
	g->mainthref = L;
	g->uvhead.prev = &g->uvhead;
	g->uvhead.next = &g->uvhead;
	random_hex_file_extension(g->vmsuffix, VM_SUFFIX_LENGTH);
	g->datastate = datastate;
	g->dataroot = NULL;

	/*
	 * Classy hack.
	 * If state_cpluaopen will fail, state_close_state will try to clean
	 * up the mess. since strhash->mask is bucket vector size minus one,
	 * 1) if uj_strhash_init did not work, ~0 + 1 == 0 and vector of 0
	 *    length will be freed.
	 * 2) if it did work, actual vector will be freed.
	 */
	strhash->mask = ~(size_t)0;
	strhash_sealed->mask = ~(size_t)0;
	g->strhash_sweep = strhash;
	g->strhash_hit = 0;
	g->strhash_miss = 0;
	memset(g->gc.state_count, 0, GCSlast * sizeof(size_t));

	setnilV(registry(L));
	setnilV(&g->nilnode.val);
	setnilV(&g->nilnode.key);
	uj_sbuf_init(NULL, &g->tmpbuf);
	g->gc.state = GCSpause;
	g->gc.root = obj2gco(L);
	g->gc.sweep = &g->gc.root;
	g->gc.pause = LUAI_GCPAUSE;
	g->gc.stepmul = LUAI_GCMUL;
	uj_dispatch_init((struct GG_State *)L);
	/* Avoid touching the stack upon memory error. */
	uj_state_add_event(L, EXTEV_VM_INIT);
	cpluaopen_status = lj_vm_cpcall(L, NULL, NULL, state_cpluaopen);
	uj_state_remove_event(L, EXTEV_VM_INIT);
	if (cpluaopen_status != 0) {
		/* Memory allocation error: free partial state. */
		state_close_state(L);
		return NULL;
	}

	g->strempty_own.marked = LJ_GC_WHITE0;
	g->strempty_own.gct = ~LJ_TSTR;
	if (NULL != datastate) {
		g->strempty = G(datastate)->strempty;
	} else {
		/*
		 * If this state will be used as DataState, strempty_own will
		 * be forwarded to all dependent states, which will have
		 * asynchronious GC cycles and current white colors. In order
		 * to mitigate it and not trigger isdead assertion, we're
		 * sealing strempty_own. It does not affect anything, as it
		 * will not be collected anyway.
		 */
		GCstr *strempty_own = &g->strempty_own;
		uj_obj_immutable_set_mark(obj2gco(strempty_own));
		uj_obj_seal(L, obj2gco(strempty_own));
		g->strempty = strempty_own;
	}

#if LJ_HASJIT
	g->argbuf = &g->argbuf_head;
	g->argbuf->base = &g->argbuf_slots[0];
#endif /* LJ_HASJIT */

	L->status = 0;
	L->events = 0;
	G(L)->panic = state_panic;
#ifdef UJIT_COVERAGE
	g->coverage = NULL;
#endif /* UJIT_COVERAGE */
	g->enable_itern = opt != NULL ? !opt->disableitern : 1;

	/*
	 * Just an extra check that some fields that are supposed to stay
	 * persistent were not erroneously touched during the initialization.
	 */
	lua_assert(0 == g->stremptyz);
	lua_assert('\0' == (strdata(&g->strempty_own))[0]);
	lua_assert(NULL == g->nullobj);

	return L;
}

static TValue *state_cpfinalize(lua_State *L, lua_CFunction dummy, void *ud)
{
	UNUSED(dummy);
	UNUSED(ud);
	lj_gc_finalize_cdata(L);
	lj_gc_finalize_udata(L);
	/* Frame pop omitted. */
	return NULL;
}

void uj_state_close(lua_State *L)
{
	global_State *g = G(L);
	int i;

	L = mainthread(g); /* Only the main thread can be closed. */
	uj_upval_close(L, L->stack);
	/* Separate udata which have GC metamethods. */
	lj_gc_separateudata(g, 1);
#if LJ_HASJIT
	G2J(g)->flags &= ~JIT_F_ON;
	G2J(g)->state = LJ_TRACE_IDLE;
	uj_dispatch_update(g);
#endif
	for (i = 0;;) {
		hook_enter(g);
		L->status = 0;
		L->cframe = NULL;
		L->base = L->top = L->stack + 1;
		if (lj_vm_cpcall(L, NULL, NULL, state_cpfinalize) == 0) {
			if (++i >= 10)
				break;
			/* Separate udata again. */
			lj_gc_separateudata(g, 1);
			/* Until nothing is left to do. */
			if (g->gc.mmudata == NULL)
				break;
		}
	}
	state_close_state(L);
}

lua_State *uj_state_new(lua_State *L)
{
	lua_State *L1 = (lua_State *)uj_obj_new(L, sizeof(lua_State));

#ifdef UJIT_IPROF_ENABLED
	L1->iprof = NULL;
#endif /* UJIT_IPROF_ENABLED */
	L1->gct = ~LJ_TTHREAD;
	L1->dummy_ffid = FF_C;
	memset(&L1->timeout, 0, sizeof(struct coro_timeout));
	L1->status = 0;
	L1->events = 0;
	L1->stacksize = 0;
	L1->stack = NULL;
	L1->cframe = NULL;
	/* NOBARRIER: The lua_State is new (marked white). */
	L1->openupval = NULL;
	L1->glref = L->glref;
	L1->env = L->env;
	state_stack_init(L1, L); /* init stack */
	lua_assert(iswhite(obj2gco(L1)));
	return L1;
}

void uj_state_setexpticks(lua_State *L)
{
	uint64_t usec;

	if (!uj_state_has_timeout(L))
		return;

	usec = L->timeout.usec;

	if (uj_timerint_is_resolvable_usec(usec)) {
		const uint64_t nticks = uj_timerint_to_ticks(usec);
		L->timeout.expticks = nticks + uj_timerint_ticks();
	} else {
		L->timeout.expticks = 0; /* Expire ASAP, almost immediately. */
	}
}

static LJ_AINLINE int state_aborted(const lua_State *L)
{
	return L->status > LUA_YIELD;
}

/* Check if a timeout can be set for the given coroutine. */
static int state_settimeout_validate(const lua_State *L,
				     const struct timeval *timeout, int restart)
{
	if (NULL == timeout || !uj_timerint_is_valid(timeout))
		return LUAE_TIMEOUT_ERRBADTIME;

	if (!uj_timerint_is_ticking())
		return LUAE_TIMEOUT_ERRNOTICKS;

	if (hook_active(G(L)))
		return LUAE_TIMEOUT_ERRHOOK;

	if (state_aborted(L))
		return LUAE_TIMEOUT_ERRABORT;

	if (L == mainthread(G(L)))
		return LUAE_TIMEOUT_ERRMAIN;

	/*
	 * Coroutine is in a valid state (running or yielded), and already has
	 * a timeout: It's illegal to change the timeout unless a restart
	 * is requested explicitly.
	 */
	if (!restart && uj_state_has_timeout(L))
		return LUAE_TIMEOUT_ERRRUNNING;

	return LUAE_TIMEOUT_SUCCESS;
}

int uj_state_settimeout(lua_State *L, const struct timeval *timeout,
			int restart)
{
	int status = state_settimeout_validate(L, timeout, restart);

	if (status != LUAE_TIMEOUT_SUCCESS)
		return status;

	L->timeout.usec = uj_timerint_to_usec(timeout);

	if (restart)
		uj_state_setexpticks(L);

	return status;
}

lua_CFunction uj_state_settimeout_callback(lua_State *L, lua_CFunction callback)
{
	lua_CFunction old_callback = L->timeout.callback;

	L->timeout.callback = callback;
	return old_callback;
}

void uj_state_free(global_State *g, lua_State *L)
{
	uj_upval_close(L, L->stack);
	lua_assert(L->openupval == NULL);
	state_stack_free(g, L);

	if (LJ_LIKELY(L != mainthread(g))) {
		/*
		 * NB! Main coroutine must *NOT* be freed here because
		 * it was allocated within GG_State.
		 */
#ifdef UJIT_IPROF_ENABLED
		uj_iprof_release(L);
#endif /* UJIT_IPROF_ENABLED */
		uj_mem_free(MEM_G(g), L, sizeof(*L));
	}
}
