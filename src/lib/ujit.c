/*
 * Lua interface to uJIT-specific extensions to the public Lua/C API.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "lua.h"
#include "lextlib.h"

#include "uj_lib.h"
#include "uj_err.h"
#include "lj_tab.h"
#include "uj_str.h"
#include "uj_sbuf.h"
#include "lj_obj.h"
#include "uj_dispatch.h"
#include "uj_vmstate.h"
#include "uj_meta.h"
#include "profile/uj_profile_iface.h"
#include "profile/uj_memprof_iface.h"
#include "profile/uj_iprof_iface.h"
#include "lj_debug.h"
#include "uj_ff.h"
#include "lj_gc.h"
#include "uj_state.h"
#include "uj_coverage.h"
#include "utils/lj_char.h"

static LJ_AINLINE void setnumfield(struct lua_State *L, struct GCtab *t,
				   const char *name, int64_t val)
{
	setnumV(lj_tab_setstr(L, t, uj_str_newz(L, name)), (double)val);
}
/* ------------------------------------------------------------------------ */

#define LJLIB_MODULE_ujit

/*
 * uJIT-specific extension: Wrapper around luaE_metrics call
 * local metrics = ujit.getmetrics()
 */
LJLIB_CF(ujit_getmetrics)
{
	struct luae_Metrics m_raw = luaE_metrics(L);
	struct GCtab *m;

	lua_createtable(L, 0, 16);
	m = tabV(L->top - 1);

	setnumfield(L, m, "strnum", m_raw.strnum);
	setnumfield(L, m, "tabnum", m_raw.tabnum);
	setnumfield(L, m, "udatanum", m_raw.udatanum);

	setnumfield(L, m, "gc_total", m_raw.gc_total);
	setnumfield(L, m, "gc_sealed", m_raw.gc_sealed);

	setnumfield(L, m, "gc_freed", m_raw.gc_freed);
	setnumfield(L, m, "gc_allocated", m_raw.gc_allocated);

	setnumfield(L, m, "gc_steps_pause", m_raw.gc_steps_pause);
	setnumfield(L, m, "gc_steps_propagate", m_raw.gc_steps_propagate);
	setnumfield(L, m, "gc_steps_atomic", m_raw.gc_steps_atomic);
	setnumfield(L, m, "gc_steps_sweepstring", m_raw.gc_steps_sweepstring);
	setnumfield(L, m, "gc_steps_sweep", m_raw.gc_steps_sweep);
	setnumfield(L, m, "gc_steps_finalize", m_raw.gc_steps_finalize);

	setnumfield(L, m, "jit_snap_restore", m_raw.jit_snap_restore);

	setnumfield(L, m, "strhash_hit", m_raw.strhash_hit);
	setnumfield(L, m, "strhash_miss", m_raw.strhash_miss);

	return 1;
}

/*
 * ujit.seal(t) -- makes t sealed in place
 * local t = ujit.seal(t) -- returns its first argument for convenience
 */
LJLIB_CF(ujit_seal)
{
	luaE_seal(L, 1);
	lua_settop(L, 1);
	return 1;
}

/*
 * ujit.immutable(t) -- makes t immutable in place
 * local t = ujit.ummutable(t) -- returns its first argument for convenience
 */
LJLIB_CF(ujit_immutable) LJLIB_REC(.)
{
	luaE_immutable(L, 1);
	lua_settop(L, 1);
	return 1;
}

LJLIB_CF(ujit_usesfenv)
{
	int result;

	uj_lib_checkfunc(L, 1);
	result = luaE_usesfenv(L, 1);
	setboolV(L->top++, result);
	return 1;
}

#include "lj_libdef.h"

/* ----- common aux code for ujit.{dump,profile} modules  ----------------- */

#define STDOUT_PSEUDO_NAME "-"

/*
 * Assuming that narg'th slot in current Lua frame is a string s, performs
 * s = s .. vmsuffix
 * where vmsuffix is VM-specific string introduced for matching results of
 * various debugging functions with each other. For convenience, returns a
 * pointer to the raw data.
 */
static const char *fname_vmsuffix(struct lua_State *L, unsigned int narg)
{
	struct sbuf *sb = uj_sbuf_reset_tmp(L);
	union TValue *name = uj_lib_narg2tv(L, narg);
	struct GCstr *s = strV(name);
	const char *vmsuffix = G(L)->vmsuffix;

	uj_sbuf_reserve(sb, s->len + VM_SUFFIX_SIZE);
	uj_sbuf_push_str(sb, s);
	uj_sbuf_push_cstr(sb, vmsuffix);
	s = uj_str_frombuf(L, sb);
	setstrV(L, name, s);
	return strdata(s);
}

/* ----- ujit.dump module ------------------------------------------------- */

#define LJLIB_MODULE_ujit_dump

#include "dump/uj_dump_iface.h"
#include "dump/uj_dump_utils.h"

static LJ_AINLINE int isnonnegnum(lua_Number num)
{
	return lj_fp_finite(num) && num >= 0.0;
}

static LJ_AINLINE int tvisnonnegnum(const union TValue *tv)
{
	return tvisnum(tv) && isnonnegnum(numV(tv));
}

static size_t get_pc(struct lua_State *L, unsigned int narg)
{
	lua_Number pc = uj_lib_checknum(L, narg);

	if (LJ_UNLIKELY(!isnonnegnum(pc) || pc > LJ_MAX_BCINS))
		uj_err_caller(L, UJ_ERR_DUMPBCINS_BADPC);

	return (size_t)pc;
}

#if LJ_HASJIT
/* Returns trace object by its number stored in the given slot of Lua stack. */
static GCtrace *get_trace_object(struct lua_State *L, unsigned int narg)
{
	struct jit_State *J = L2J(L);
	TraceNo tr = uj_lib_checkint(L, narg);

	if (tr <= 0 || tr >= J->sizetrace)
		return NULL;

	return traceref(J, tr);
}
#endif /* LJ_HASJIT */

/* ujit.dump.stack(io_object) */
LJLIB_CF(ujit_dump_stack)
{
	struct IOFileUD *iof = uddata(uj_lib_checkiofile(L, 1));

	uj_dump_stack(iof->fp, L);
	return 0;
}

/* ujit.dump.bc(io_object, func) */
LJLIB_CF(ujit_dump_bc)
{
	struct IOFileUD *iof = uddata(uj_lib_checkiofile(L, 1));
	const union GCfunc *fn = uj_lib_checkfunc(L, 2);

	uj_dump_bc(iof->fp, fn);
	return 0;
}

/* ujit.dump.bcins(io_object, func, pc[, nest_level]) */
LJLIB_CF(ujit_dump_bcins)
{
	struct IOFileUD *iof = uddata(uj_lib_checkiofile(L, 1));
	const union GCfunc *fn = uj_lib_checkfunc(L, 2);
	uint8_t lvl = 0;

	if (lua_gettop(L) > 3) {
		const union TValue *slot_lvl = uj_lib_narg2tv(L, 4);

		if (tvisnonnegnum(slot_lvl))
			lvl = (uint8_t)numV(slot_lvl);
	}

	if (isluafunc(fn)) {
		size_t pc = get_pc(L, 3);
		const struct GCproto *pt = funcproto(fn);

		if (pc < pt->sizebc) {
			uj_dump_bc_ins(iof->fp, pt, proto_bc(pt) + pc, lvl);
			lua_pushboolean(L, 1);
		} else {
			lua_pushboolean(L, 0);
		}
		return 1;
	}

	/*
	 * If a non-Lua function is passed, we ignore pc and simply
	 * dump its single pseudo-bc instruction.
	 */
	uj_dump_nonlua_bc_ins(iof->fp, fn, lvl);
	lua_pushboolean(L, 1);
	return 1;
}

/* ujit.dump.trace(io_object, trace_no) */
LJLIB_CF(ujit_dump_trace)
{
#if LJ_HASJIT
	struct IOFileUD *iof = uddata(uj_lib_checkiofile(L, 1));
	const struct GCtrace *trace = get_trace_object(L, 2);

	if (trace == NULL)
		return 0;

	uj_dump_ir(iof->fp, trace);
#else
	UNUSED(L);
#endif /* LJ_HASJIT */

	return 0;
}

/* ujit.dump.mcode(io_object, trace_no) */
LJLIB_CF(ujit_dump_mcode)
{
#if LJ_HASJIT
	struct IOFileUD *iof = uddata(uj_lib_checkiofile(L, 1));
	const struct GCtrace *trace = get_trace_object(L, 2);

	if (trace == NULL)
		return 0;

	uj_dump_mcode(iof->fp, L2J(L), trace);
#else
	UNUSED(L);
#endif /* LJ_HASJIT */

	return 0;
}

/*
 * local started, fname_real = ujit.dump.start([fname_stub])
 * fname_stub is suffixed with VM-specific suffix to ensure interoperability
 * with other debugging functions and is returned to the caller as fname_real.
 * If fname_stub is omitted or passed as "-", dumping is started to stdout
 * and fname_real is set to "-", too.
 */
LJLIB_CF(ujit_dump_start)
{
#if LJ_HASJIT
	int dump_status;
	FILE *out = stdout;
	const struct GCstr *fname = uj_lib_optstr(L, 1);

	if (fname != NULL && strcmp(strdata(fname), STDOUT_PSEUDO_NAME) != 0) {
		const char *name = fname_vmsuffix(L, 1);

		out = fopen(name, "w");
		if (out == NULL)
			uj_err_callerv(L, UJ_ERR_DUMP_BADFILE, name);
	}

	lua_assert(out != NULL);

	dump_status = uj_dump_start(L, out);
	lua_pushboolean(L, dump_status == 0 ? 1 : 0);

	if (LJ_LIKELY(dump_status == 0)) {
		if (LJ_LIKELY(out != stdout))
			lua_pushvalue(L, 1);
		else
			lua_pushstring(L, STDOUT_PSEUDO_NAME);
	} else {
		lua_pushnil(L);
	}
#else
	lua_pushboolean(L, 0);
	lua_pushboolean(L, 0);
#endif /* LJ_HASJIT */

	return 2;
}

/* local stopped = ujit.dump.stop() */
LJLIB_CF(ujit_dump_stop)
{
#if LJ_HASJIT
	lua_pushboolean(L, uj_dump_stop(L) == 0 ? 1 : 0);
#else
	lua_pushboolean(L, 0);
#endif /* LJ_HASJIT */

	return 1;
}

/* ------------------------------------------------------------------------ */

#include "lj_libdef.h"

/* ----- ujit.table module ---------------------------------------------- */

#define LJLIB_MODULE_ujit_table

LJLIB_CF(ujit_table_shallowcopy) LJLIB_REC(.)
{
	uj_lib_checktab(L, 1);
	luaE_shallowcopytable(L, 1);

	return 1;
}

LJLIB_CF(ujit_table_keys) LJLIB_REC(.)
{
	uj_lib_checktab(L, 1);
	luaE_tablekeys(L, 1);

	return 1;
}

LJLIB_CF(ujit_table_values) LJLIB_REC(.)
{
	uj_lib_checktab(L, 1);
	luaE_tablevalues(L, 1);

	return 1;
}

LJLIB_CF(ujit_table_toset) LJLIB_REC(.)
{
	uj_lib_checktab(L, 1);
	luaE_tabletoset(L, 1);

	return 1;
}

LJLIB_CF(ujit_table_size) LJLIB_REC(.)
{
	const GCtab *t = uj_lib_checktab(L, 1);
	lua_pushinteger(L, lj_tab_size(t));

	return 1;
}

/*
 * local value = ujit.table.rindex(t, ...) -- recursively indexes t with ...
 */
LJLIB_CF(ujit_table_rindex) LJLIB_REC(.)
{
	const TValue *tv = NULL;
	const size_t narg = (size_t)lua_gettop(L);
	int fast_path = 0;

	if (LJ_UNLIKELY(narg == 0 || tvisnil(L->base))) {
		lua_pushnil(L);
		return 1;
	}

	/*
	 * TODO: Ideally, an interface like this should throw if an input is not a
	 * table. But current implementation has to serve as a drop-in
	 * replacement for some other API. Need to deprecate this quirk some
	 * day.
	 */
	if (tvistab(L->base)) {
		if (narg == 1) {
			lua_pushnil(L);
			return 1;
		}

		tv = lj_tab_rawrindex(L, L->base, narg);
		if (tv != NULL) {
			fast_path = 1;
			goto out;
		}
	}

	/*
	 * TODO: Currently the slow path is twice as slow in the worst case as
	 * intermediate lookup results are completely discarded. Let's fix if it
	 * becomes a real performance issue.
	 */
	tv = uj_meta_rindex(L, narg);

out:
	setboolV(&G(L)->tmptv2, fast_path); /* Remember for trace recorder. */
	copyTV(L, L->top, tv);
	uj_state_stack_incr_top(L);
	return 1;
}

/* ------------------------------------------------------------------------ */

#include "lj_libdef.h"

/* ----- ujit.profile module ---------------------------------------------- */

/*
 * Assuming that there is a string s on top of the stack, performs
 * s = s .. vmsuffix
 * where vmsuffix is VM-specific string introduced for matching results of
 * various debugging functions with each other. For convenience, returns a
 * pointer to the raw data.
 */

#define LJLIB_MODULE_ujit_profile

static void store_vmstate_counter(struct lua_State *L, struct GCtab *t,
				  const struct profiler_data *c,
				  enum vmstate vmst)
{
	setnumfield(L, t, uj_vmstate_names[vmst], c->vmstate[vmst]);
}

/* local available = ujit.profile.available() */
LJLIB_CF(ujit_profile_available)
{
	lua_pushboolean(L, uj_profile_available() == LUAE_PROFILE_SUCCESS);
	return 1;
}

/* local initialized = ujit.profile.init() */
LJLIB_CF(ujit_profile_init)
{
	lua_pushboolean(L, uj_profile_init() == LUAE_PROFILE_SUCCESS);
	return 1;
}

/* local terminated = ujit.profile.terminate() */
LJLIB_CF(ujit_profile_terminate)
{
	lua_pushboolean(L, uj_profile_terminate() == LUAE_PROFILE_SUCCESS);
	return 1;
}

static enum profiler_mode profile_parse_mode(struct lua_State *L,
					     unsigned int narg)
{
	const char *mode = strdata(uj_lib_checkstr(L, narg));

	if (!strcmp(mode, "default"))
		return PROFILE_DEFAULT;
	if (!strcmp(mode, "leaf"))
		return PROFILE_LEAF;
	if (!strcmp(mode, "callgraph"))
		return PROFILE_CALLGRAPH;
	uj_err_caller(L, UJ_ERR_PROF_START_BADMODE);
}

/*
 * local started, fname_real = ujit.profile.start(interval, mode[, fname_stub])
 * No default value for interval. Throws an error if interval is not a number,
 * or a number outside the range [1; UINT32_MAX].
 * If fname_stub is omitted or nil, collects lightweight in-memory per-VM state
 * profile. Otherwise collects per-VM state profile *and* leaf profile. In this
 * case, fname_stub is suffixed with VM-specific suffix to ensure
 * interoperability with other debugging functions and is returned to the caller
 * as fname_real. fname_real is used for streaming collected leaf profile.
 * If fname_real already exists, it is recreated. Throws if fname_stub is set,
 * but fname_real cannot be open for writing for any reason. If fname_stub is
 * omitted or nil, fname_real is nil.
 */
LJLIB_CF(ujit_profile_start)
{
	struct profiler_options opt = {0};
	const char *fname;
	int fd = -1;
	int started;
	enum profiler_mode mode;
	lua_Number interval = uj_lib_checknum(L, 1);

	if (LJ_UNLIKELY(interval < 1.0 || interval > (lua_Number)UINT32_MAX))
		uj_err_caller(L, UJ_ERR_PROF_START_BADIVL);

	opt.interval = (uint32_t)interval;
	opt.mode = mode = profile_parse_mode(L, 2);

	if (mode != PROFILE_DEFAULT) {
		uj_lib_checkstr(L, 3);
		fname = fname_vmsuffix(L, 3);
		fd = open(fname, O_CREAT | O_WRONLY | O_TRUNC, 0644);
		if (fd == -1)
			uj_err_callerv(L, UJ_ERR_PROF_START_BADFILE, fname);
	}

	started = uj_profile_start(L, &opt, fd) == LUAE_PROFILE_SUCCESS;
	lua_pushboolean(L, started);

	if (LJ_LIKELY(started)) {
		if (mode != PROFILE_DEFAULT)
			lua_pushvalue(L, 3);
		else
			lua_pushnil(L);
	} else {
		if (mode != PROFILE_DEFAULT) {
			close(fd);
			remove(fname);
		}
		lua_pushnil(L);
	}

	return 2;
}

#define PROFILE_ID_BUFFER_SIZE 20

/*
 * local counters[, err_reason] = ujit.profile.stop()
 * On sucess, returns a table with in-memory VM counters.
 * On failure, returns nil as the first argument and an error reason string
 * as the second argument.
 */
LJLIB_CF(ujit_profile_stop)
{
	struct profiler_data counters;
	char profile_id[PROFILE_ID_BUFFER_SIZE] = {0};
	int stop_status;
	struct GCtab *t_cnt;

	if (uj_profile_report(&counters) != LUAE_PROFILE_SUCCESS) {
		lua_pushnil(L);
		lua_pushliteral(L, "Error fetching VM state counters");
		return 2;
	}

	stop_status = uj_profile_stop();
	if (stop_status != LUAE_PROFILE_SUCCESS) {
		lua_pushnil(L);
		switch (stop_status) {
		case LUAE_PROFILE_ERRIO:
			lua_pushliteral(
				L,
				"I/O error: Event stream is most likely broken");
			break;
		case LUAE_PROFILE_ERRMEM:
			lua_pushliteral(
				L, "Not enough memory for stream profiling");
			break;
		default:
			lua_pushliteral(L, "Internal error");
			break;
		}
		return 2;
	}

	lua_createtable(L, 0, 16);
	t_cnt = tabV(L->top - 1);
	store_vmstate_counter(L, t_cnt, &counters, UJ_VMST_IDLE);
	store_vmstate_counter(L, t_cnt, &counters, UJ_VMST_INTERP);
	store_vmstate_counter(L, t_cnt, &counters, UJ_VMST_LFUNC);
	store_vmstate_counter(L, t_cnt, &counters, UJ_VMST_FFUNC);
	store_vmstate_counter(L, t_cnt, &counters, UJ_VMST_CFUNC);
	store_vmstate_counter(L, t_cnt, &counters, UJ_VMST_GC);
	store_vmstate_counter(L, t_cnt, &counters, UJ_VMST_EXIT);
	store_vmstate_counter(L, t_cnt, &counters, UJ_VMST_RECORD);
	store_vmstate_counter(L, t_cnt, &counters, UJ_VMST_OPT);
	store_vmstate_counter(L, t_cnt, &counters, UJ_VMST_ASM);

	setnumfield(L, t_cnt, "TRACE", counters.vmstate[UJ_VMST_TRACE]);
	setnumfield(L, t_cnt, "_SAMPLES", counters.num_samples);
	setnumfield(L, t_cnt, "_NUM_OVERRUNS", counters.num_overruns);

	/* t_cnt["_ID"] = tostring(counters.id) */
	sprintf(profile_id, "%#018llx", (unsigned long long)counters.id);
	lua_pushstring(L, profile_id);
	lua_setfield(L, -2, "_ID");

	return 1;
}

#include "lj_libdef.h"

/* ----- ujit.memprof module ---------------------------------------------- */

#define LJLIB_MODULE_ujit_memprof

/* local started, fname_real = ujit.memprof.start(interval, fname_stub) */
LJLIB_CF(ujit_memprof_start)
{
	struct memprof_options opt = {0};
	const char *fname;
	int started;

	opt.dursec = (uint64_t)uj_lib_checknum(L, 1);

	uj_lib_checkstr(L, 2);
	fname = fname_vmsuffix(L, 2);

	opt.fd = open(fname, O_CREAT | O_WRONLY | O_TRUNC, 0644);

	if (opt.fd == -1) {
		lua_pushboolean(L, 0);
		lua_pushnil(L);
		return 2;
	}

	started = uj_memprof_start(L, &opt) == LUAE_PROFILE_SUCCESS;
	lua_pushboolean(L, started);

	if (LJ_LIKELY(started)) {
		lua_pushvalue(L, 2);
	} else {
		close(opt.fd);
		remove(fname);
		lua_pushnil(L);
	}

	return 2;
}

/* local stopped = ujit.memprof.stop() */
LJLIB_CF(ujit_memprof_stop)
{
	lua_pushboolean(L, uj_memprof_stop() == LUAE_PROFILE_SUCCESS);
	return 1;
}

#include "lj_libdef.h"

/* ----- ujit.coverage module --------------------------------------------- */

#define LJLIB_MODULE_ujit_coverage

/* local started = ujit.coverage.start(filename[, excludes]) */
LJLIB_CF(ujit_coverage_start)
{
	size_t exclude_size;
	const char **excludes;
	int res;
	const struct GCstr *filename = uj_lib_checkstr(L, 1);
	const struct GCtab *opttab = uj_lib_opttab(L, 2);

	if (opttab != NULL) {
		int i;

		exclude_size = lj_tab_len(opttab);
		excludes = uj_mem_alloc(L, exclude_size * sizeof(char *));
		/* Lua arrays start from '1' index */
		for (i = 1; i <= exclude_size; i++)
			excludes[i - 1] = strVdata(lj_tab_getint(opttab, i));
	} else {
		exclude_size = 0;
		excludes = NULL;
	}

	res = uj_coverage_start(L, strdata(filename), excludes, exclude_size);
	lua_pushboolean(L, res == LUA_COV_SUCCESS);
	if (exclude_size > 0)
		uj_mem_free(MEM(L), excludes, exclude_size * sizeof(char *));
	return 1;
}

/* ujit.coverage.stop() */
LJLIB_CF(ujit_coverage_stop)
{
	uj_coverage_stop(L);
	return 0;
}

/* ujit.coverage.pause() */
LJLIB_CF(ujit_coverage_pause)
{
	uj_coverage_pause(L);
	return 0;
}

/* ujit.coverage.unpause() */
LJLIB_CF(ujit_coverage_unpause)
{
	uj_coverage_unpause(L);
	return 0;
}

#include "lj_libdef.h"

/* ----- ujit.iprof module ------------------------------------------------ */

#define LJLIB_MODULE_ujit_iprof

static enum iprof_mode iprof_optmode(struct lua_State *L, unsigned int narg)
{
	GCstr *arg = uj_lib_optstr(L, narg);
	const char *mode = arg ? strdata(arg) : "plain";

	if (strcmp(mode, "plain") == 0)
		return IPROF_PLAIN;
	uj_err_caller(L, UJ_ERR_IPROF_START_BADMODE);
}

static const char *iprof_optname(struct lua_State *L, unsigned int narg)
{
	int size;
	TValue *frame;
	const char *name, *what;
	GCstr *arg = uj_lib_optstr(L, narg);

	if (arg)
		return strdata(arg);

	frame = (TValue *)lj_debug_frame(L, 1, &size);
	what = frame ? lj_debug_funcname(L, frame, &name) : NULL;
	return what ? name : "main";
}

LJLIB_CF(ujit_iprof_start)
{
	switch (uj_iprof_start(L, iprof_optname(L, 1), iprof_optmode(L, 2))) {
	case IPROF_SUCCESS: {
		lua_pushboolean(L, 1);
		return 1;
	}
	case IPROF_ERRMEM: {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "Not enough memory for profiling");
		return 2;
	}
	case IPROF_ERRNYI: {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "Inappropriate ujit build");
		return 2;
	}
	default: {
		/* Unhandled error */
		lua_assert(0);
		/* Unreachable */
		return 0;
	}
	}
}

LJLIB_CF(ujit_iprof_stop)
{
	GCtab *result = NULL;

	lj_gc_check(L);

	switch (uj_iprof_stop(L, &result)) {
	case IPROF_SUCCESS: {
		settabV(L, L->top, result);
		uj_state_stack_incr_top(L);
		return 1;
	}
	case IPROF_ERRMEM: {
		lua_pushnil(L);
		lua_pushliteral(L, "Not enough memory for profiling");
		return 2;
	}
	case IPROF_ERRNYI: {
		lua_pushnil(L);
		lua_pushliteral(L, "Inappropriate ujit build");
		return 2;
	}
	default: {
		/* Unhandled error */
		lua_assert(0);
		/* Unreachable */
		return 0;
	}
	}
}

LJLIB_PUSH("plain")
LJLIB_SET(PLAIN)

#include "lj_libdef.h"

/* ----- ujit.math module ------------------------------------------------- */

#define LJLIB_MODULE_ujit_math

LJLIB_CF(ujit_math_ispinf) LJLIB_REC(.)
{
	lua_Number n = uj_lib_checknum(L, 1);
	int b = (uj_fp_classify(n) == LJ_FP_PINF);

	lua_pushboolean(L, b);
	setboolV(&G(L)->tmptv2, b); /* Remember for trace recorder. */
	return 1;
}

LJLIB_CF(ujit_math_isninf) LJLIB_REC(.)
{
	lua_Number n = uj_lib_checknum(L, 1);
	int b = (uj_fp_classify(n) == LJ_FP_MINF);

	lua_pushboolean(L, b);
	setboolV(&G(L)->tmptv2, b); /* Remember for trace recorder. */
	return 1;
}

LJLIB_CF(ujit_math_isinf) LJLIB_REC(.)
{
	lua_Number n = uj_lib_checknum(L, 1);
	FpType t = uj_fp_classify(n);
	int b = (t == LJ_FP_PINF || t == LJ_FP_MINF);

	lua_pushboolean(L, b);
	setboolV(&G(L)->tmptv2, b); /* Remember for trace recorder. */
	return 1;
}

LJLIB_PUSH(NAN)
LJLIB_SET(nan)

LJLIB_CF(ujit_math_isnan) LJLIB_REC(.)
{
	lua_Number n = uj_lib_checknum(L, 1);
	int b = (uj_fp_classify(n) == LJ_FP_NAN);

	lua_pushboolean(L, b);
	setboolV(&G(L)->tmptv2, b); /* Remember for trace recorder. */
	return 1;
}

LJLIB_CF(ujit_math_isfinite) LJLIB_REC(.)
{
	lua_Number n = uj_lib_checknum(L, 1);
	int b = (uj_fp_classify(n) == LJ_FP_FINITE);

	lua_pushboolean(L, b);
	setboolV(&G(L)->tmptv2, b); /* Remember for trace recorder. */
	return 1;
}

#include "lj_libdef.h"

/* ----- ujit.debug module ------------------------------------------------ */

#define LJLIB_MODULE_ujit_debug

/* local table_info = ujit.debug.gettableinfo(t) */
LJLIB_CF(ujit_debug_gettableinfo)
{
	struct tab_info ti;
	const GCtab *t = uj_lib_checktab(L, 1);
	GCtab *info;

	lj_tab_getinfo(L, t, &ti);

	lua_createtable(L, 0, 8);
	info = tabV(L->top - 1);
	setnumfield(L, info, "acapacity", (int64_t)ti.acapacity);
	setnumfield(L, info, "asize", (int64_t)ti.asize);
	setnumfield(L, info, "hcapacity", (int64_t)ti.hcapacity);
	setnumfield(L, info, "hsize", (int64_t)ti.hsize);
	setnumfield(L, info, "hnchains", (int64_t)ti.hnchains);
	setnumfield(L, info, "hmaxchain", (int64_t)ti.hmaxchain);

	return 1;
}

#include "lj_libdef.h"

/* ----- ujit.string module ------------------------------------------------- */

#define LJLIB_MODULE_ujit_string

LJLIB_CF(ujit_string_trim) LJLIB_REC(.)
{
	GCstr *s = uj_lib_checkstr(L, 1);
	lj_gc_check(L);

	setstrV(L, L->top, uj_str_trim(L, s));
	uj_state_stack_incr_top(L);

	return 1;
}

LJLIB_NOREG LJLIB_CF(ujit_string_split_aux)
{
	size_t i, j;
	const char *find_res = NULL;
	GCstr *token;

	/* input string */
	const GCstr *str = uj_lib_checkstr(L, 1);

	/* upvalues */
	const GCstr *sep = strV(uj_lib_upvalue(L, 1));
	TValue *i_tv = uj_lib_upvalue(L, 2);

	i = rawV(i_tv);
	if (i > str->len) {
		setnilV(L->top);
		uj_state_stack_incr_top(L);
		return 1;
	}

	/* i == str->len only when str->len == 0 */
	if (i < str->len)
		find_res = uj_cstr_find(strdata(str) + i, strdata(sep),
					str->len - i, sep->len);

	if (find_res != NULL)
		j = (size_t)(find_res - strdata(str));
	else
		j = str->len;

	/* update control variable */
	setrawV(i_tv, (uint64_t)(j + sep->len));

	/* return token */
	lua_assert(j <= str->len);
	token = uj_str_new(L, strdata(str) + i, j - i);
	setstrV(L, L->top, token);
	uj_state_stack_incr_top(L);

	lj_gc_check(L);
	return 1;
}

LJLIB_CF(ujit_string_split)
{
	GCstr *str = uj_lib_checkstr(L, 1);
	GCstr *sep = uj_lib_checkstr(L, 2);

	if (sep->len == 0)
		uj_err_arg(L, UJ_ERR_SPLIT_EMPTY_SEPARATOR, 2);

	/* separator, 1st upvalue */
	setstrV(L, L->top, sep);
	uj_state_stack_incr_top(L);

	/* control variable, 2nd upvalue */
	setrawV(L->top, 0);
	uj_state_stack_incr_top(L);

	/* push closure on stack */
	uj_lib_pushcc(L, lj_cf_ujit_string_split_aux, FF_ujit_string_split_aux,
		      2);

	/* push input string on stack */
	setstrV(L, L->top, str);
	uj_state_stack_incr_top(L);

	/* return splitter_closure, str */
	return 2;
}

#include "lj_libdef.h"

LUALIB_API int luaopen_ujit(struct lua_State *L)
{
	LJ_LIB_REG(L, LUAE_UJITLIBNAME, ujit);
	LJ_LIB_REG(L, "ujit.profile", ujit_profile);
	LJ_LIB_REG(L, "ujit.memprof", ujit_memprof);
	LJ_LIB_REG(L, "ujit.dump", ujit_dump);
	LJ_LIB_REG(L, "ujit.table", ujit_table);
	LJ_LIB_REG(L, "ujit.coverage", ujit_coverage);
	LJ_LIB_REG(L, "ujit.iprof", ujit_iprof);
	LJ_LIB_REG(L, "ujit.math", ujit_math);
	LJ_LIB_REG(L, "ujit.debug", ujit_debug);
	LJ_LIB_REG(L, "ujit.string", ujit_string);
	return 1;
}
