/*
 * uJIT-specific extensions to the public Lua/C API.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "lua.h"
#include "lauxlib.h"
#include "lextlib.h"
#include "ujit.h"
#include "uj_capi_impl.h"

#include "uj_mem.h"
#include "uj_dispatch.h"
#include "lj_obj.h"
#include "uj_obj_seal.h"
#include "uj_obj_immutable.h"
#include "lj_gc.h"
#include "uj_state.h"
#include "lj_tab.h"
#include "uj_func.h"
#include "uj_timerint.h"
#include "uj_coverage.h"
#include "utils/uj_alloc.h"
#include "profile/uj_profile_iface.h"
#include "dump/uj_dump_iface.h"

/* Invalid bytecode position. (see lj_debug.c) */
#define NO_BCPOS (~(BCPos)0)

static const char *const uj_verstring = UJIT_VERSION_STRING;

LUAEXT_API const char *luaE_verstring(void)
{
	return uj_verstring;
}

LUAEXT_API void luaE_seal(lua_State *L, int idx)
{
	/*
	 * Current implementation explicitly performs a full garbage-collection
	 * cycle, pauses GC and performs actual sealing while GC is paused.
	 * This is done to mitigate issues that occur when sealing clashes with
	 * certain GC phases.  As soon as GC is paused, sealed objects get
	 * marked recursively and the object chain is relinked to preserve the
	 * sealing invariant. After that, GC is explicitly restarted.
	 * Implementation may be suboptimal performance-wise, but stability
	 * comes first.  Any attempt to optimize this function *MUST* be
	 * accompanied with tests.
	 */
	TValue *tv = uj_capi_index2adr(L, idx);

	if (!tvisgcv(tv))
		return;

	api_check(L, !gl_datastate(G(L)));

	lua_gc(L, LUA_GCCOLLECT, 0);
	lua_gc(L, LUA_GCSTOP, 0);
	lua_assert(G(L)->gc.state == GCSpause);

	uj_obj_seal(L, gcV(tv));

	lua_gc(L, LUA_GCRESTART, -1);
}

LUAEXT_API void luaE_immutable(lua_State *L, int idx)
{
	TValue *tv = uj_capi_index2adr(L, idx);

	if (!tvisgcv(tv))
		return;

	uj_obj_immutable(L, gcV(tv));
}

LUAEXT_API struct luae_Metrics luaE_metrics(lua_State *L)
{
	struct luae_Metrics rv;
	const struct mem_metrics *mem_metrics;
	global_State *g = G(L);
	GCState *gc = &g->gc;
#if LJ_HASJIT
	jit_State *J = G2J(g);
#endif

	rv.strhash_hit = g->strhash_hit;
	g->strhash_hit = 0;

	rv.strhash_miss = g->strhash_miss;
	g->strhash_miss = 0;

	uj_strhash_t *strhash = gl_strhash(g);
	rv.strnum = strhash->count;
	rv.tabnum = gc->tabnum;
	rv.udatanum = gc->udatanum;

	rv.gc_sealed = gc->sealed;

	mem_metrics = uj_mem_metrics(MEM(L));
	rv.gc_total = mem_metrics->total;
	rv.gc_freed = mem_metrics->freed;
	rv.gc_allocated = mem_metrics->allocated;
	uj_mem_flush_metrics(MEM(L));

	rv.gc_steps_pause = gc->state_count[GCSpause];
	gc->state_count[GCSpause] = 0;

	rv.gc_steps_propagate = gc->state_count[GCSpropagate];
	gc->state_count[GCSpropagate] = 0;

	rv.gc_steps_atomic = gc->state_count[GCSatomic];
	gc->state_count[GCSatomic] = 0;

	rv.gc_steps_sweepstring = gc->state_count[GCSsweepstring];
	gc->state_count[GCSsweepstring] = 0;

	rv.gc_steps_sweep = gc->state_count[GCSsweep];
	gc->state_count[GCSsweep] = 0;

	rv.gc_steps_finalize = gc->state_count[GCSfinalize];
	gc->state_count[GCSfinalize] = 0;

#if LJ_HASJIT
	rv.jit_snap_restore = J->nsnaprestore;
	J->nsnaprestore = 0;

	rv.jit_mcode_size = J->szallmcarea;
	rv.jit_trace_num = J->freetrace;
#else
	rv.jit_snap_restore = 0;
	rv.jit_mcode_size = 0;
	rv.jit_trace_num = 0;
#endif

	return rv;
}

LUAEXT_API lua_State *luaE_newdataslave(lua_State *datastate)
{
	struct luae_Options opt = {0};

	opt.datastate = datastate;
	return uj_state_newstate(&opt);
}

/* Used in DataState to save data root. */
LUAEXT_API void luaE_setdataroot(lua_State *L, int idx)
{
	global_State *g = G(L);
	const TValue *o = uj_capi_index2adr(L, idx);
	api_check(L, tvistab(o));
	api_check(L, !gl_datastate(g));

	/* Overwrite forbidden for no apparent reason. */
	api_check(L, NULL == g->dataroot);

	g->dataroot = tabV(o);
}

/* Used in regular state to register data root from DataState. */
LUAEXT_API void luaE_getdataroot(lua_State *L)
{
	lua_State *Ld = gl_datastate(G(L));
	api_check(L, NULL != Ld);
	settabV(L, L->top, G(Ld)->dataroot);
	uj_state_stack_incr_top(L);
}

LUAEXT_API lua_State *luaE_createstate(const struct luae_Options *opt)
{
	return uj_state_newstate(opt);
}

LUAEXT_API size_t luaE_totalmem(void)
{
	struct alloc_stats stats = uj_alloc_stats();
	return stats.active;
}

/* undocumented */
LUAEXT_API struct alloc_stats luaE_allocstats(void)
{
	return uj_alloc_stats();
}

LUAEXT_API int luaE_usesfenv(lua_State *L, int idx)
{
	const TValue *tv = uj_capi_index2adr(L, idx);

	api_check(L, tvisfunc(tv));
	return uj_func_usesfenv(funcV(tv));
}

LUAEXT_API int luaE_profavailable(void)
{
	return uj_profile_available();
}

LUAEXT_API int luaE_profinit(void)
{
	return uj_profile_init();
}

LUAEXT_API int luaE_profterm(void)
{
	return uj_profile_terminate();
}

LUAEXT_API int luaE_dumpstart(const lua_State *L, FILE *out)
{
#if LJ_HASJIT
	return uj_dump_start(L, out);
#else
	UNUSED(L);
	UNUSED(out);
	return 0;
#endif
}

LUAEXT_API int luaE_dumpstop(const lua_State *L)
{
#if LJ_HASJIT
	return uj_dump_stop(L);
#else
	UNUSED(L);
	return 0;
#endif
}

LUAEXT_API void luaE_dumpbc(lua_State *L, int idx, FILE *out)
{
	TValue *tv = uj_capi_index2adr(L, idx);

	if (!tvisfunc(tv))
		return;

	uj_dump_bc(out, funcV(tv));
}

LUAEXT_API void luaE_dumpbcsource(lua_State *L, int idx, FILE *out,
				  int hl_bc_pos)
{
	TValue *tv = uj_capi_index2adr(L, idx);

	if (!tvisfunc(tv))
		return;

	BCPos pos = (hl_bc_pos != -1) ? (BCPos)hl_bc_pos : NO_BCPOS;
	uj_dump_bc_and_source(out, funcV(tv), pos);
}

LUAEXT_API uint64_t luaE_iterate(lua_State *L, int idx, uint64_t iter_state)
{
	const TValue *t = uj_capi_index2adr(L, idx);

	api_check(L, tvistab(t));
	lua_assert(iter_state == (uint32_t)iter_state);

	return (uint64_t)lj_tab_iterate(L, tabV(t), (uint32_t)iter_state);
}

LUAEXT_API int luaE_intinit(int signo)
{
	return uj_timerint_init(signo);
}

LUAEXT_API int luaE_intterm(void)
{
	return uj_timerint_terminate();
}

LUAEXT_API int luaE_intresolvable(const struct timeval *timeout)
{
	return uj_timerint_is_resolvable_usec(uj_timerint_to_usec(timeout));
}

LUAEXT_API lua_CFunction luaE_settimeoutf(lua_State *L, lua_CFunction callback)
{
	return uj_state_settimeout_callback(L, callback);
}

LUAEXT_API int luaE_settimeout(lua_State *L, const struct timeval *timeout,
			       int restart)
{
	return uj_state_settimeout(L, timeout, restart);
}

typedef struct GCtab *(*create_tab_func)(struct lua_State *,
					 const struct GCtab *);

static LJ_AINLINE void capi_ext_newtable(lua_State *L, int idx,
					 create_tab_func create_tab_f)
{
	const TValue *tv = uj_capi_index2adr(L, idx);
	const GCtab *tab;
	GCtab *newtab;

	api_check(L, tvistab(tv));

	tab = tabV(tv);
	lj_gc_check(L);
	newtab = create_tab_f(L, tab);

	settabV(L, L->top, newtab);
	uj_state_stack_incr_top(L);
}

LUAEXT_API void luaE_shallowcopytable(lua_State *L, int idx)
{
	capi_ext_newtable(L, idx, lj_tab_dup);
}

LUAEXT_API void luaE_tablekeys(lua_State *L, int idx)
{
	capi_ext_newtable(L, idx, lj_tab_keys);
}

LUAEXT_API void luaE_tablevalues(lua_State *L, int idx)
{
	capi_ext_newtable(L, idx, lj_tab_values);
}

LUAEXT_API void luaE_tabletoset(lua_State *L, int idx)
{
	capi_ext_newtable(L, idx, lj_tab_toset);
}

LUAEXT_API void luaE_deepcopytable(lua_State *to, lua_State *from, int idx)
{
	const TValue *tv = uj_capi_index2adr(from, idx);
	GCtab *dst, *src;
	struct deepcopy_ctx ctx;

	api_check(from, tvistab(tv));

	src = tabV(tv);
	lua_createtable(to, 0, 8);
	ctx.map = tabV(to->top - 1);
	lua_newtable(to);
	ctx.env = tabV(to->top - 1);

	lj_gc_check(to);
	dst = lj_tab_deepcopy(to, src, &ctx);

	to->top = to->top - 2; /* Free env and map */
	uj_obj_clear_mark(from, obj2gco(src));

	settabV(to, to->top, dst);
	uj_state_stack_incr_top(to);
}

LUAEXT_API int luaE_coveragestart(lua_State *L, const char *filename,
				  const char **excludes, size_t num)
{
	return uj_coverage_start(L, filename, excludes, num);
}

LUAEXT_API int luaE_coveragestart_cb(lua_State *L, lua_Coveragewriter cb,
				     void *context, const char **excludes,
				     size_t num)
{
	return uj_coverage_start_cb(L, cb, context, excludes, num);
}

LUAEXT_API int luaE_coveragestop(lua_State *L)
{
	return uj_coverage_stop(L);
}

LUAEXT_API void luaE_requiref(lua_State *L, const char *modname,
			      lua_CFunction openf)
{
	lua_pushcfunction(L, openf);
	lua_pushstring(L, modname); /* argument to open function */
	lua_call(L, 1, 1); /* open module */
	luaL_findtable(L, LUA_REGISTRYINDEX, "_LOADED", 16);
	lua_pushvalue(L, -2); /* make copy of module (call result) */
	lua_setfield(L, -2, modname); /* _LOADED[modname] = module */
	lua_pop(L, 1); /* remove _LOADED table */
}
