/*
 * Public Lua/C API.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 *
 * Major portions taken verbatim or adapted from the Lua interpreter.
 * Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
 */

#include "lua.h"
#include "lextlib.h"

#include "lj_obj.h"
#include "uj_mem.h"
#include "lj_gc.h"
#include "uj_dispatch.h"
#include "uj_err.h"
#include "uj_throw.h"
#include "uj_errmsg.h"
#include "lj_debug.h"
#include "uj_str.h"
#include "lj_tab.h"
#include "uj_func.h"
#include "uj_proto.h"
#include "uj_udata.h"
#include "uj_meta.h"
#include "uj_mtab.h"
#include "uj_state.h"
#include "lj_bc.h"
#include "lj_frame.h"
#include "jit/lj_trace.h"
#include "lj_vm.h"
#include "uj_cframe.h"
#include "frontend/lj_lex.h"
#include "frontend/lj_parse.h"
#include "lj_bcdump.h"
#include "uj_hotcnt.h"

/* -- Common helper  --------------------------------------------- */

#define api_checknelems(L, n) api_check((L), (n) <= ((L)->top - (L)->base))
#define api_checkvalidindex(L, i) api_check((L), (i) != niltv(L))

TValue *uj_capi_index2adr(lua_State *L, int idx)
{
	if (idx > 0) {
		TValue *o = L->base + (idx - 1);
		return o < L->top ? o : niltv(L);
	} else if (idx > LUA_REGISTRYINDEX) {
		api_check(L, idx != 0 && -idx <= L->top - L->base);
		return L->top + idx;
	} else if (idx == LUA_GLOBALSINDEX) {
		TValue *o = &G(L)->tmptv;
		settabV(L, o, L->env);
		return o;
	} else if (idx == LUA_REGISTRYINDEX) {
		return registry(L);
	} else {
		GCfunc *fn = curr_func(L);
		api_check(L, fn->c.gct == ~LJ_TFUNC && !isluafunc(fn));
		if (idx == LUA_ENVIRONINDEX) {
			TValue *o = &G(L)->tmptv;
			settabV(L, o, fn->c.env);
			return o;
		} else {
			idx = LUA_GLOBALSINDEX - idx;
			return idx <= fn->c.nupvalues ? &fn->c.upvalue[idx - 1]
						      : niltv(L);
		}
	}
}

static TValue *stkindex2adr(lua_State *L, int idx)
{
	if (idx > 0) {
		TValue *o = L->base + (idx - 1);
		return o < L->top ? o : niltv(L);
	} else {
		api_check(L, idx != 0 && -idx <= L->top - L->base);
		return L->top + idx;
	}
}

static GCtab *getcurrenv(lua_State *L)
{
	GCfunc *fn = curr_func(L);
	return fn->c.gct == ~LJ_TFUNC ? fn->c.env : L->env;
}

/* --  API functions ----------------------------------------- */

LUA_API int lua_status(lua_State *L)
{
	return L->status;
}

LUA_API int lua_checkstack(lua_State *L, int size)
{
	if (size > LUAI_MAXCSTACK || (L->top - L->base + size) > LUAI_MAXCSTACK)
		return 0; /* Stack overflow. */

	if (size > 0)
		uj_state_stack_check(L, (size_t)size);
	return 1;
}

LUA_API void lua_xmove(lua_State *from, lua_State *to, int n)
{
	TValue *f;
	TValue *t;
	if (from == to)
		return;
	api_checknelems(from, n);
	api_check(from, G(from) == G(to));
	uj_state_stack_check(to, (size_t)n);
	f = from->top;
	t = to->top = to->top + n;
	while (--n >= 0)
		copyTV(to, --t, --f);
	from->top = f;
}

/* -- Stack manipulation -------------------------------------------------- */

LUA_API int lua_gettop(lua_State *L)
{
	return (int)(L->top - L->base);
}

LUA_API void lua_settop(lua_State *L, int idx)
{
	if (idx < 0) {
		/* Shrink stack via negative index. */
		api_check(L, -(idx + 1) <= (L->top - L->base));
		L->top += idx + 1;
		return;
	}

	api_check(L, idx <= L->maxstack - L->base);

	if (L->base + idx <= L->top) {
		/* Shrink stack via positive index. */
		L->top = L->base + idx;
		return;
	}

	/* Grow stack. L->top is repositioned inside uj_state_stack_grow. */
	if (L->base + idx >= L->maxstack)
		uj_state_stack_grow(L,
				    (size_t)idx - (size_t)(L->top - L->base));

	do {
		setnilV(L->top++);
	} while (L->top < L->base + idx);
}

LUA_API void lua_remove(lua_State *L, int idx)
{
	TValue *p = stkindex2adr(L, idx);
	api_checkvalidindex(L, p);
	while (++p < L->top)
		copyTV(L, p - 1, p);
	L->top--;
}

LUA_API void lua_insert(lua_State *L, int idx)
{
	TValue *q, *p = stkindex2adr(L, idx);
	api_checkvalidindex(L, p);
	for (q = L->top; q > p; q--)
		copyTV(L, q, q - 1);
	copyTV(L, p, L->top);
}

LUA_API void lua_replace(lua_State *L, int idx)
{
	api_checknelems(L, 1);
	if (idx == LUA_GLOBALSINDEX) {
		api_check(L, tvistab(L->top - 1));
		/* NOBARRIER: A thread (i.e. L) is never black. */
		L->env = tabV(L->top - 1);
	} else if (idx == LUA_ENVIRONINDEX) {
		GCfunc *fn = curr_func(L);
		if (fn->c.gct != ~LJ_TFUNC)
			uj_err(L, UJ_ERR_NOENV);
		api_check(L, tvistab(L->top - 1));
		fn->c.env = tabV(L->top - 1);
		lj_gc_barrier(L, fn, L->top - 1);
	} else {
		TValue *o = uj_capi_index2adr(L, idx);
		api_checkvalidindex(L, o);
		copyTV(L, o, L->top - 1);
		if (idx < LUA_GLOBALSINDEX) /* Need a barrier for upvalues. */
			lj_gc_barrier(L, curr_func(L), L->top - 1);
	}
	L->top--;
}

LUA_API void lua_pushvalue(lua_State *L, int idx)
{
	copyTV(L, L->top, uj_capi_index2adr(L, idx));
	uj_state_stack_incr_top(L);
}

/* -- Stack getters ------------------------------------------------------- */

LUA_API int lua_type(lua_State *L, int idx)
{
	const TValue *o = uj_capi_index2adr(L, idx);
	if (tvisnum(o)) {
		return LUA_TNUMBER;
	} else if (tvislightud(o)) {
		return LUA_TLIGHTUSERDATA;
	} else if (o == niltv(L)) {
		return LUA_TNONE;
	} else { /* Magic internal/external tag conversion. ORDER LJ_T */
		uint32_t t = ~gettag(o);
		int tt = (int)((U64x(75a06, 98042110) >> 4 * t) & 15u);
		lua_assert(tt != LUA_TNIL || tvisnil(o));
		return tt;
	}
}

LUA_API const char *lua_typename(lua_State *L, int t)
{
	UNUSED(L);
	return uj_obj_typename[t + 1];
}

LUA_API int lua_iscfunction(lua_State *L, int idx)
{
	const TValue *o = uj_capi_index2adr(L, idx);
	return tvisfunc(o) && !isluafunc(funcV(o));
}

LUA_API int lua_isnumber(lua_State *L, int idx)
{
	const TValue *o = uj_capi_index2adr(L, idx);
	TValue tmp;
	return (tvisnum(o) || (tvisstr(o) && uj_str_tonumtv(strV(o), &tmp)));
}

LUA_API int lua_isstring(lua_State *L, int idx)
{
	const TValue *o = uj_capi_index2adr(L, idx);
	return (tvisstr(o) || tvisnum(o));
}

LUA_API int lua_isuserdata(lua_State *L, int idx)
{
	const TValue *o = uj_capi_index2adr(L, idx);
	return (tvisudata(o) || tvislightud(o));
}

LUA_API int lua_rawequal(lua_State *L, int idx1, int idx2)
{
	const TValue *o1 = uj_capi_index2adr(L, idx1);
	const TValue *o2 = uj_capi_index2adr(L, idx2);
	return (o1 == niltv(L) || o2 == niltv(L)) ? 0 : uj_obj_equal(o1, o2);
}

LUA_API int lua_equal(lua_State *L, int idx1, int idx2)
{
	const TValue *o1 = uj_capi_index2adr(L, idx1);
	const TValue *o2 = uj_capi_index2adr(L, idx2);
	if (tvisnum(o1) && tvisnum(o2)) {
		return numV(o1) == numV(o2);
	} else if (gettag(o1) != gettag(o2)) {
		return 0;
	} else if (tvispri(o1)) {
		return o1 != niltv(L) && o2 != niltv(L);
	} else if (tvislightud(o1)) {
		return rawnumequal(o1, o2);
	} else if (gcval(o1) == gcval(o2)) {
		return 1;
	} else if (!tvistabud(o1)) {
		return 0;
	} else {
		TValue *base = uj_meta_equal(L, gcV(o1), gcV(o2), 0);
		if ((uintptr_t)base <= 1) {
			return (int)(uintptr_t)base;
		} else {
			uj_meta_mmcall(L, base, uj_mm_narg[MM_eq], 1);
			L->top -= uj_mm_narg[MM_eq];
			return tvistruecond(L->top + 1);
		}
	}
}

LUA_API int lua_lessthan(lua_State *L, int idx1, int idx2)
{
	const TValue *o1 = uj_capi_index2adr(L, idx1);
	const TValue *o2 = uj_capi_index2adr(L, idx2);
	if (o1 == niltv(L) || o2 == niltv(L)) {
		return 0;
	} else if (tvisnum(o1) && tvisnum(o2)) {
		return numV(o1) < numV(o2);
	} else {
		TValue *base = uj_meta_comp(L, o1, o2, 0);
		if ((uintptr_t)base <= 1) {
			return (int)(uintptr_t)base;
		} else {
			uj_meta_mmcall(L, base, uj_mm_narg[MM_lt], 1);
			L->top -= uj_mm_narg[MM_lt];
			return tvistruecond(L->top + 1);
		}
	}
}

LUA_API lua_Number lua_tonumber(lua_State *L, int idx)
{
	const TValue *o = uj_capi_index2adr(L, idx);
	TValue tmp;
	if (LJ_LIKELY(tvisnum(o)))
		return numV(o);
	else if (tvisstr(o) && uj_str_tonumtv(strV(o), &tmp))
		return numV(&tmp);
	else
		return 0;
}

LUA_API lua_Integer lua_tointeger(lua_State *L, int idx)
{
	return (lua_Integer)lua_tonumber(L, idx);
}

LUA_API int lua_toboolean(lua_State *L, int idx)
{
	const TValue *o = uj_capi_index2adr(L, idx);
	return tvistruecond(o);
}

LUA_API const char *lua_tolstring(lua_State *L, int idx, size_t *len)
{
	TValue *o = uj_capi_index2adr(L, idx);
	GCstr *s;
	if (LJ_LIKELY(tvisstr(o))) {
		s = strV(o);
	} else if (tvisnum(o)) {
		lj_gc_check(L);
		o = uj_capi_index2adr(L, idx); /* GC may move the stack. */
		s = uj_str_fromnumber(L, o->n);
		setstrV(L, o, s);
	} else {
		if (len != NULL)
			*len = 0;
		return NULL;
	}
	if (len != NULL)
		*len = s->len;
	return strdata(s);
}

LUA_API size_t lua_objlen(lua_State *L, int idx)
{
	TValue *o = uj_capi_index2adr(L, idx);
	if (tvisstr(o)) {
		return strV(o)->len;
	} else if (tvistab(o)) {
		return (size_t)lj_tab_len(tabV(o));
	} else if (tvisudata(o)) {
		return udataV(o)->len;
	} else if (tvisnum(o)) {
		GCstr *s = uj_str_fromnumber(L, o->n);
		setstrV(L, o, s);
		return s->len;
	} else {
		return 0;
	}
}

LUA_API lua_CFunction lua_tocfunction(lua_State *L, int idx)
{
	const TValue *o = uj_capi_index2adr(L, idx);
	if (tvisfunc(o)) {
		BCOp op = bc_op(*(funcV(o)->c.pc));

		if (op == BC_FUNCC || op == BC_FUNCCW)
			return funcV(o)->c.f;
	}
	return NULL;
}

LUA_API void *lua_touserdata(lua_State *L, int idx)
{
	const TValue *o = uj_capi_index2adr(L, idx);
	if (tvisudata(o))
		return uddata(udataV(o));
	else if (tvislightud(o))
		return lightudV(o);
	else
		return NULL;
}

LUA_API lua_State *lua_tothread(lua_State *L, int idx)
{
	const TValue *o = uj_capi_index2adr(L, idx);
	return (!tvisthread(o)) ? NULL : threadV(o);
}

LUA_API const void *lua_topointer(lua_State *L, int idx)
{
	const TValue *o = uj_capi_index2adr(L, idx);
	if (tvisudata(o))
		return uddata(udataV(o));
	else if (tvislightud(o))
		return lightudV(o);
	else if (tviscdata(o))
		return cdataptr(cdataV(o));
	else if (tvisgcv(o))
		return gcV(o);
	else
		return NULL;
}

/* -- Stack setters (object creation) ------------------------------------- */

LUA_API void lua_pushnil(lua_State *L)
{
	setnilV(L->top);
	uj_state_stack_incr_top(L);
}

LUA_API void lua_pushnumber(lua_State *L, lua_Number n)
{
	setnumV(L->top, n);
	if (LJ_UNLIKELY(tvisnan(L->top)))
		setnanV(L->top); /* Canonicalize injected NaNs. */
	uj_state_stack_incr_top(L);
}

LUA_API void lua_pushinteger(lua_State *L, lua_Integer n)
{
	setintptrV(L->top, n);
	uj_state_stack_incr_top(L);
}

LUA_API void lua_pushlstring(lua_State *L, const char *str, size_t len)
{
	GCstr *s;
	lj_gc_check(L);
	s = uj_str_new(L, str, len);
	setstrV(L, L->top, s);
	uj_state_stack_incr_top(L);
}

LUA_API void lua_pushstring(lua_State *L, const char *str)
{
	if (str == NULL) {
		setnilV(L->top);
	} else {
		GCstr *s;
		lj_gc_check(L);
		s = uj_str_newz(L, str);
		setstrV(L, L->top, s);
	}
	uj_state_stack_incr_top(L);
}

LUA_API const char *lua_pushvfstring(lua_State *L, const char *fmt,
				     va_list argp)
{
	lj_gc_check(L);
	return uj_str_pushvf(L, fmt, argp);
}

LUA_API const char *lua_pushfstring(lua_State *L, const char *fmt, ...)
{
	const char *ret;
	va_list argp;
	lj_gc_check(L);
	va_start(argp, fmt);
	ret = uj_str_pushvf(L, fmt, argp);
	va_end(argp);
	return ret;
}

LUA_API void lua_pushcclosure(lua_State *L, lua_CFunction f, int n)
{
	GCfunc *fn;
	lj_gc_check(L);
	api_checknelems(L, n);
	fn = uj_func_newC(L, (size_t)n, getcurrenv(L));
	fn->c.f = f;
	L->top -= n;
	while (n--)
		copyTV(L, &fn->c.upvalue[n], L->top + n);
	setfuncV(L, L->top, fn);
	lua_assert(iswhite(obj2gco(fn)));
	uj_state_stack_incr_top(L);
}

LUA_API void lua_pushboolean(lua_State *L, int b)
{
	setboolV(L->top, (b != 0));
	uj_state_stack_incr_top(L);
}

LUA_API void lua_pushlightuserdata(lua_State *L, void *p)
{
	setlightudV(L->top, p);
	uj_state_stack_incr_top(L);
}

LUA_API void lua_createtable(lua_State *L, int narray, int nrec)
{
	GCtab *t;
	uint32_t asize = (uint32_t)(narray > 0 ? narray + 1 : 0);
	lj_gc_check(L);
	t = lj_tab_new(L, asize, hsize2hbits(nrec));
	settabV(L, L->top, t);
	uj_state_stack_incr_top(L);
}

LUA_API int lua_pushthread(lua_State *L)
{
	setthreadV(L, L->top, L);
	uj_state_stack_incr_top(L);
	return (mainthread(G(L)) == L);
}

LUA_API lua_State *lua_newthread(lua_State *L)
{
	lua_State *L1;
	lj_gc_check(L);
	L1 = uj_state_new(L);
	setthreadV(L, L->top, L1);
	uj_state_stack_incr_top(L);
	return L1;
}

LUA_API void *lua_newuserdata(lua_State *L, size_t size)
{
	GCudata *ud;
	lj_gc_check(L);
	if (size > LJ_MAX_UDATA)
		uj_err(L, UJ_ERR_UDATAOV);
	ud = uj_udata_new(L, (size_t)size, getcurrenv(L));
	setudataV(L, L->top, ud);
	uj_state_stack_incr_top(L);
	return uddata(ud);
}

LUA_API void lua_concat(lua_State *L, int n)
{
	/*
	 * Relative positions are used because a call to metamethod can
	 * unpredictably invoke a GC which may reallocate the stack.
	 */
	ptrdiff_t rel_bottom;
	ptrdiff_t rel_top;

	api_checknelems(L, n);

	if (n < 2) {
		if (n == 0) {
			setstrV(L, L->top, G(L)->strempty);
			uj_state_stack_incr_top(L);
		} /* else n == 1: nothing to do. */
		return;
	}

	rel_bottom = uj_state_stack_save(L, L->top - n);
	rel_top = uj_state_stack_save(L, L->top - 1);
	while (rel_top > rel_bottom) {
		TValue *bottom = uj_state_stack_restore(L, rel_bottom);
		TValue *top = uj_state_stack_restore(L, rel_top);
		top = uj_meta_cat(L, bottom, top);

		if (top == NULL) {
			top = bottom;
		} else {
			uj_meta_mmcall(L, top, uj_mm_narg[MM_concat], 1);
			copyTV(L, L->top - 2, L->top - 1);
			L->top -= 1;
			top = L->top - 1;
		}

		rel_top = uj_state_stack_save(L, top);
	}

	lua_assert(rel_top == rel_bottom);
	L->top = uj_state_stack_restore(L, rel_bottom) + 1;

	lj_gc_check(L);
	return;
}

/* -- Object getters ------------------------------------------------------ */

/* Get name and value of upvalue. */
static const char *capi_uv(const TValue *tv, int n, TValue **tvp)
{
	GCfunc *fn;
	uint32_t uv;

	if (!tvisfunc(tv))
		return NULL;

	fn = funcV(tv);
	uv = (uint32_t)(n - 1);
	if (isluafunc(fn)) {
		const GCproto *pt = funcproto(fn);

		if (uv < pt->sizeuv) {
			*tvp = uvval(fn->l.uvptr[uv]);
			return uj_proto_uvname(pt, uv);
		}
	} else {
		if (uv < fn->c.nupvalues) {
			*tvp = &fn->c.upvalue[uv];
			return "";
		}
	}

	return NULL;
}

/* Get name of local variable from 1-based slot number and function/frame. */
static TValue *capi_localname(lua_State *L, const struct lua_Debug *ar,
			      const char **name, int slot1)
{
	uint32_t offset = (uint32_t)ar->i_ci & 0xffff;
	uint32_t size = (uint32_t)ar->i_ci >> 16;
	TValue *frame = L->stack + offset;
	const TValue *nextframe = size ? frame + size : NULL;
	GCfunc *fn = frame_func(frame);
	BCPos pos = lj_debug_framepc(L, fn, nextframe);

	if (nextframe == NULL)
		nextframe = L->top;

	if (slot1 < 0) { /* Negative slot number is for varargs. */
		const GCproto *pt;

		if (pos == NO_BCPOS)
			return NULL;

		pt = funcproto(fn);
		if ((pt->flags & PROTO_VARARG)) {
			slot1 = (int)pt->numparams - slot1;

			if (frame_isvarg(frame)) {
				/* Vararg frame has been set up? (pos != 0) */
				nextframe = frame;
				frame = frame_prevd(frame);
			}

			if (frame + slot1 < nextframe) {
				*name = "(*vararg)";
				return frame + slot1;
			}
		}

		return NULL;
	}

	if (pos != NO_BCPOS) {
		*name = uj_proto_varname(funcproto(fn), pos, slot1 - 1);
		if (*name != NULL)
			return frame + slot1;
	}

	if (slot1 > 0 && frame + slot1 < nextframe)
		*name = "(*temporary)";

	return frame + slot1;
}

static void capi_get_field(lua_State *L, int idx, const TValue *k, int ret_idx)
{
	const TValue *t;
	const TValue *v;

	t = uj_capi_index2adr(L, idx);
	api_checkvalidindex(L, t);
	v = uj_meta_tget(L, t, k);

	if (NULL == v)
		v = uj_meta_index(L);

	copyTV(L, L->top - ret_idx, v);
}

LUA_API void lua_gettable(lua_State *L, int idx)
{
	capi_get_field(L, idx, L->top - 1, 1);
}

LUA_API void lua_getfield(lua_State *L, int idx, const char *k)
{
	TValue key;

	setstrV(L, &key, uj_str_newz(L, k));
	capi_get_field(L, idx, &key, 0);
	uj_state_stack_incr_top(L);
}

LUA_API void lua_rawget(lua_State *L, int idx)
{
	const TValue *t = uj_capi_index2adr(L, idx);
	api_check(L, tvistab(t));
	copyTV(L, L->top - 1, lj_tab_get(L, tabV(t), L->top - 1));
}

LUA_API void lua_rawgeti(lua_State *L, int idx, int n)
{
	const TValue *v;
	const TValue *t = uj_capi_index2adr(L, idx);
	api_check(L, tvistab(t));
	v = lj_tab_getint(tabV(t), n);
	if (v)
		copyTV(L, L->top, v);
	else
		setnilV(L->top);
	uj_state_stack_incr_top(L);
}

LUA_API int lua_getmetatable(lua_State *L, int idx)
{
	const TValue *o = uj_capi_index2adr(L, idx);
	GCtab *mt = uj_mtab_get(L, o);
	if (mt == NULL)
		return 0;
	settabV(L, L->top, mt);
	uj_state_stack_incr_top(L);
	return 1;
}

LUA_API void lua_getfenv(lua_State *L, int idx)
{
	const TValue *o = uj_capi_index2adr(L, idx);
	api_checkvalidindex(L, o);
	if (tvisfunc(o)) {
		settabV(L, L->top, funcV(o)->c.env);
	} else if (tvisudata(o)) {
		settabV(L, L->top, udataV(o)->env);
	} else if (tvisthread(o)) {
		settabV(L, L->top, threadV(o)->env);
	} else {
		setnilV(L->top);
	}
	uj_state_stack_incr_top(L);
}

LUA_API int lua_next(lua_State *L, int idx)
{
	const TValue *t = uj_capi_index2adr(L, idx);
	int more;
	api_check(L, tvistab(t));
	more = lj_tab_next(L, tabV(t), L->top - 1);
	if (more)
		uj_state_stack_incr_top(L); /* Return new key and value slot. */
	else /* End of traversal. */
		L->top--; /* Remove key slot. */
	return more;
}

LUA_API const char *lua_getupvalue(lua_State *L, int idx, int n)
{
	TValue *val;
	TValue *fn = uj_capi_index2adr(L, idx);
	const char *name = capi_uv(fn, n, &val);
	if (name) {
		copyTV(L, L->top, val);
		uj_state_stack_incr_top(L);
	}
	return name;
}

LUA_API void *lua_upvalueid(lua_State *L, int idx, int n)
{
	GCfunc *fn = funcV(uj_capi_index2adr(L, idx));
	n--;
	api_check(L, (uint32_t)n < fn->l.nupvalues);
	return isluafunc(fn) ? (void *)(fn->l.uvptr[n])
			     : (void *)&fn->c.upvalue[n]; /* TODO: check! */
}

LUA_API void lua_upvaluejoin(lua_State *L, int idx1, int n1, int idx2, int n2)
{
	GCfunc *fn1 = funcV(uj_capi_index2adr(L, idx1));
	GCfunc *fn2 = funcV(uj_capi_index2adr(L, idx2));
	n1--;
	n2--;
	api_check(L, isluafunc(fn1) && (uint32_t)n1 < fn1->l.nupvalues);
	api_check(L, isluafunc(fn2) && (uint32_t)n2 < fn2->l.nupvalues);
	fn1->l.uvptr[n1] = fn2->l.uvptr[n2];
	lj_gc_objbarrier(L, fn1, fn1->l.uvptr[n1]);
}

/* -- Object setters ------------------------------------------------------ */

static void capi_set_field(lua_State *L, int idx, const TValue *k,
			   const TValue *v, int key_on_stack)
{
	TValue *o;
	const TValue *t;
	int nelems = key_on_stack ? 2 : 1;

	t = uj_capi_index2adr(L, idx);
	api_checknelems(L, nelems);
	api_checkvalidindex(L, t);

	o = uj_meta_tset(L, t, k);

	if (o) {
		/* NOBARRIER: uj_meta_tset ensures the table is not black. */
		copyTV(L, o, v);
		L->top -= nelems;
	} else {
		copyTV(L, L->top + 2, L->top - 3);
		uj_meta_mmcall(L, L->top, uj_mm_narg[MM_newindex], 0);
		L->top -= key_on_stack ? uj_mm_narg[MM_newindex] : 2;
	}
}

LUA_API void lua_settable(lua_State *L, int idx)
{
	capi_set_field(L, idx, L->top - 2, L->top - 1, 1);
}

LUA_API void lua_setfield(lua_State *L, int idx, const char *k)
{
	TValue key;
	setstrV(L, &key, uj_str_newz(L, k));

	capi_set_field(L, idx, &key, L->top - 1, 0);
}

LUA_API void lua_rawset(lua_State *L, int idx)
{
	GCtab *t = tabV(uj_capi_index2adr(L, idx));
	TValue *dst;
	TValue *key;
	api_checknelems(L, 2);
	key = L->top - 2;
	dst = lj_tab_set(L, t, key);
	copyTV(L, dst, key + 1);
	lj_gc_anybarriert(L, t);
	L->top = key;
}

LUA_API void lua_rawseti(lua_State *L, int idx, int n)
{
	GCtab *t = tabV(uj_capi_index2adr(L, idx));
	TValue *dst;
	TValue *src;
	api_checknelems(L, 1);
	dst = lj_tab_setint(L, t, n);
	src = L->top - 1;
	copyTV(L, dst, src);
	lj_gc_barriert(L, t, dst);
	L->top = src;
}

LUA_API int lua_setmetatable(lua_State *L, int idx)
{
	global_State *g;
	GCtab *mt;
	const TValue *o = uj_capi_index2adr(L, idx);
	api_checknelems(L, 1);
	api_checkvalidindex(L, o);
	if (tvisnil(L->top - 1)) {
		mt = NULL;
	} else {
		api_check(L, tvistab(L->top - 1));
		mt = tabV(L->top - 1);
	}
	g = G(L);
	if (tvistab(o)) {
		if (LJ_UNLIKELY(uj_obj_is_immutable(gcV(o))))
			uj_err(L, UJ_ERR_IMMUT_MODIF);
		tabV(o)->metatable = mt;
		if (mt)
			lj_gc_objbarriert(L, tabV(o), mt);
	} else if (tvisudata(o)) {
		udataV(o)->metatable = mt;
		if (mt)
			lj_gc_objbarrier(L, udataV(o), mt);
	} else {
		/* Flush cache, since traces specialize to basemt.
		 * But not during __gc.
		 */
		if (lj_trace_flushall(L))
			uj_err_caller(L, UJ_ERR_NOGCMM);

		if (tvisbool(o)) {
			/* NOBARRIER: basemt is a GC root. */
			uj_mtab_set_for_type(g, LJ_TTRUE, mt);
			uj_mtab_set_for_type(g, LJ_TFALSE, mt);
		} else {
			/* NOBARRIER: basemt is a GC root. */
			uj_mtab_set_for_otype(g, o, mt);
		}
	}
	L->top--;
	return 1;
}

LUA_API int lua_setfenv(lua_State *L, int idx)
{
	const TValue *o = uj_capi_index2adr(L, idx);
	GCtab *t;
	api_checknelems(L, 1);
	api_checkvalidindex(L, o);
	api_check(L, tvistab(L->top - 1));
	t = tabV(L->top - 1);
	if (tvisfunc(o)) {
		funcV(o)->c.env = t;
	} else if (tvisudata(o)) {
		udataV(o)->env = t;
	} else if (tvisthread(o)) {
		threadV(o)->env = t;
	} else {
		L->top--;
		return 0;
	}
	lj_gc_objbarrier(L, gcV(o), t);
	L->top--;
	return 1;
}

LUA_API const char *lua_setupvalue(lua_State *L, int idx, int n)
{
	const TValue *f = uj_capi_index2adr(L, idx);
	TValue *val;
	const char *name;
	api_checknelems(L, 1);
	name = capi_uv(f, n, &val);
	if (name) {
		L->top--;
		copyTV(L, val, L->top);
		lj_gc_barrier(L, funcV(f), L->top);
	}
	return name;
}

/* -- Calls --------------------------------------------------------------- */

LUA_API void lua_call(lua_State *L, int nargs, int nresults)
{
	api_check(L, L->status == 0 || L->status == LUA_ERRERR);
	api_checknelems(L, nargs + 1);
	lj_vm_call(L, L->top - nargs, nresults + 1);
}

LUA_API int lua_pcall(lua_State *L, int nargs, int nresults, int errfunc)
{
	global_State *g = G(L);
	uint8_t oldh = hook_save(g);
	ptrdiff_t ef;
	int status;
	api_check(L, L->status == 0 || L->status == LUA_ERRERR);
	api_checknelems(L, nargs + 1);
	if (errfunc == 0) {
		ef = 0;
	} else {
		const TValue *o = stkindex2adr(L, errfunc);
		api_checkvalidindex(L, o);
		ef = uj_state_stack_save(L, o);
	}
	status = lj_vm_pcall(L, L->top - nargs, nresults + 1, ef);
	if (status)
		hook_restore(g, oldh);
	return status;
}

static TValue *cpcall(lua_State *L, lua_CFunction func, void *ud)
{
	GCfunc *fn = uj_func_newC(L, 0, getcurrenv(L));
	fn->c.f = func;
	setfuncV(L, L->top, fn);
	setlightudV(L->top + 1, ud);
	uj_cframe_nres_set(L->cframe, 1 + 0); /* Zero results. */
	L->top += 2;
	return L->top - 1; /* Now call the newly allocated C function. */
}

LUA_API int lua_cpcall(lua_State *L, lua_CFunction func, void *ud)
{
	global_State *g = G(L);
	uint8_t oldh = hook_save(g);
	int status;
	api_check(L, L->status == 0 || L->status == LUA_ERRERR);
	status = lj_vm_cpcall(L, func, ud, cpcall);
	if (status)
		hook_restore(g, oldh);
	return status;
}

/* -- Coroutine yield and resume ------------------------------------------ */

LUA_API int lua_yield(lua_State *L, int nresults)
{
	void *cf = L->cframe;
	global_State *g = G(L);
	if (!uj_cframe_canyield(cf)) {
		uj_err(L, UJ_ERR_CYIELD);
		return 0; /* unreachable */
	}

	cf = uj_cframe_raw(cf);
	if (!hook_active(g)) { /* Regular yield: move results down if needed */
		const TValue *f = L->top - nresults;
		if (f > L->base) {
			TValue *t = L->base;
			while (--nresults >= 0)
				copyTV(L, t++, f++);
			L->top = t;
		}
		L->cframe = NULL;
		L->status = LUA_YIELD;
		return -1;
	} else { /* Yield from hook: add a pseudo-frame. */
		TValue *top = L->top;
		int64_t frame_size;
		hook_leave(g);
		setrawV(top, uj_cframe_multres(cf));
		setcont(top + 1, lj_cont_hook);
		setframe_pc(top + 1, uj_cframe_pc(cf) - 1);
		frame_size = (int64_t)((char *)(top + 3) - (char *)L->base);
		frame_set_dummy(L, top + 2, frame_size + FRAME_CONT);
		L->top = L->base = top + 3;
		uj_throw(L, LUA_YIELD);
	}
}

LUA_API int lua_resume(lua_State *L, int nargs)
{
	if (L->cframe == NULL && L->status <= LUA_YIELD) {
		if (L->status == 0) /* Initial resume */
			uj_state_setexpticks(L);
		return lj_vm_resume(L, L->top - nargs, 0, 0);
	}
	L->top = L->base;
	setstrV(L, L->top, uj_errmsg_str(L, UJ_ERR_COSUSP));
	uj_state_stack_incr_top(L);
	return LUA_ERRRUN;
}

/* -- GC and memory management -------------------------------------------- */

LUA_API int lua_gc(lua_State *L, int what, int data)
{
	global_State *g = G(L);
	int res = 0;
	size_t a;
	const size_t total = uj_mem_total(MEM(L));
	switch (what) {
	case LUA_GCSTOP:
		g->gc.threshold = LJ_MAX_MEM;
		break;
	case LUA_GCRESTART:
		g->gc.threshold = (data == -1) ? (total / 100) * g->gc.pause
					       : total;
		break;
	case LUA_GCCOLLECT:
		lj_gc_fullgc(L);
		break;
	case LUA_GCCOUNT:
		res = (int)(total >> 10);
		break;
	case LUA_GCCOUNTB:
		res = (int)(total & 0x3ff);
		break;
	case LUA_GCSTEP:
		a = (size_t)data << 10;
		g->gc.threshold = (a <= total) ? (total - a) : 0;
		while (total >= g->gc.threshold)
			if (lj_gc_step(L) > 0) {
				res = 1;
				break;
			}
		break;
	case LUA_GCSETPAUSE:
		res = (int)(g->gc.pause);
		g->gc.pause = (size_t)data;
		break;
	case LUA_GCSETSTEPMUL:
		res = (int)(g->gc.stepmul);
		g->gc.stepmul = (size_t)data;
		break;
	default:
		res = -1; /* Invalid option. */
	}
	return res;
}

LUA_API lua_Alloc lua_getallocf(lua_State *L, void **ud)
{
	struct mem_manager *mem = MEM(L);
	if (ud)
		*ud = mem->state;
	return mem->allocf;
}

LUA_API void lua_setallocf(lua_State *L, lua_Alloc f, void *ud)
{
	struct mem_manager *mem = MEM(L);
	mem->state = ud;
	mem->allocf = f;
}

LUA_API lua_State *lua_newstate(lua_Alloc f, void *ud)
{
	struct luae_Options opt = {0};

	opt.allocf = f;
	opt.allocud = ud;
	return uj_state_newstate(&opt);
}

LUA_API const char *lua_getlocal(lua_State *L, const lua_Debug *ar, int n)
{
	const char *name = NULL;
	if (ar) {
		TValue *tv = capi_localname(L, ar, &name, n);
		if (name) {
			copyTV(L, L->top, tv);
			uj_state_stack_incr_top(L);
		}
	} else if (tvisfunc(L->top - 1) && isluafunc(funcV(L->top - 1))) {
		name = uj_proto_varname(funcproto(funcV(L->top - 1)), 0, n - 1);
	}
	return name;
}

LUA_API const char *lua_setlocal(lua_State *L, const lua_Debug *ar, int n)
{
	const char *name = NULL;
	TValue *tv = capi_localname(L, ar, &name, n);
	if (name)
		copyTV(L, tv, L->top - 1);
	L->top--;
	return name;
}

LUA_API int lua_getinfo(lua_State *L, const char *what, lua_Debug *ar)
{
	return lj_debug_getinfo(L, what, (lj_Debug *)ar, 0);
}

LUA_API int lua_getstack(lua_State *L, int level, lua_Debug *ar)
{
	int size;
	const TValue *frame = lj_debug_frame(L, level, &size);
	if (frame) {
		ar->i_ci = (size << 16) + (int)(frame - L->stack);
		return 1;
	} else {
		ar->i_ci = level - size;
		return 0;
	}
}

LUA_API lua_CFunction lua_atpanic(lua_State *L, lua_CFunction panicf)
{
	lua_CFunction old = G(L)->panic;
	G(L)->panic = panicf;
	return old;
}

/* Forwarders for the public API (C calling convention and no LJ_NORET). */
LUA_API int lua_error(lua_State *L)
{
	uj_throw_run(L);
	return 0; /* unreachable */
}

/* This function can be called asynchronously (e.g. during a signal). */
LUA_API int lua_sethook(lua_State *L, lua_Hook func, int mask, int count)
{
	global_State *g = G(L);
	mask &= HOOK_EVENTMASK;
	if (func == NULL || mask == 0) {
		mask = 0;
		func = NULL;
	} /* Consistency. */
	g->hookf = func;
	g->hookcount = g->hookcstart = (int32_t)count;
	g->hookmask = (uint8_t)((g->hookmask & ~HOOK_EVENTMASK) | mask);
	lj_trace_abort(g); /* Abort recording on any hook change. */
	uj_dispatch_update(g);
	return 1;
}

LUA_API lua_Hook lua_gethook(lua_State *L)
{
	return G(L)->hookf;
}

LUA_API int lua_gethookmask(lua_State *L)
{
	return G(L)->hookmask & HOOK_EVENTMASK;
}

LUA_API int lua_gethookcount(lua_State *L)
{
	return (int)G(L)->hookcstart;
}

LUA_API void lua_close(lua_State *L)
{
	uj_state_close(L);
}

LUA_API int lua_load(lua_State *L, lua_Reader reader, void *data,
		     const char *chunkname)
{
	return lua_loadx(L, reader, data, chunkname, NULL);
}

LUA_API int lua_dump(lua_State *L, lua_Writer writer, void *data)
{
	const TValue *o = L->top - 1;
	api_check(L, L->top > L->base);
	if (tvisfunc(o) && isluafunc(funcV(o)))
		return lj_bcwrite(L, funcproto(funcV(o)), writer, data, 0);
	else
		return 1;
}

static TValue *cpparser(lua_State *L, lua_CFunction dummy, void *ud)
{
	LexState *ls = (LexState *)ud;
#if LJ_HASJIT
	jit_State *J = L2J(L);
#endif
	GCproto *pt;
	GCfunc *fn;
	int bc;
	UNUSED(dummy);
	uj_cframe_errfunc_inherit(L->cframe); /* Inherit error function. */
	bc = lj_lex_setup(L, ls);
	if (ls->mode && !strchr(ls->mode, bc ? 'b' : 't')) {
		setstrV(L, L->top++, uj_errmsg_str(L, UJ_ERR_XMODE));
		uj_throw(L, LUA_ERRSYNTAX);
	}
	pt = bc ? lj_bcread(ls) : lj_parse(ls);
	fn = uj_func_newL_empty(L, pt, L->env);
	/* Don't combine above/below into one statement. */
	setfuncV(L, L->top++, fn);
#if LJ_HASJIT
	uj_hotcnt_patch_bc(pt, (uint16_t)J->param[JIT_P_hotloop]);
#endif
	return NULL;
}

LUA_API int lua_loadx(lua_State *L, lua_Reader reader, void *data,
		      const char *chunkname, const char *mode)
{
	LexState ls;
	int status;
	ls.rfunc = reader;
	ls.rdata = data;
	ls.chunkarg = chunkname ? chunkname : "?";
	ls.mode = mode;
	status = lj_vm_cpcall(L, NULL, &ls, cpparser);
	lj_lex_cleanup(L, &ls);
	lj_gc_check(L);
	return status;
}
