/*
 * Auxiliary library for the Lua/C API.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 *
 * Major parts taken verbatim or adapted from the Lua interpreter.
 * Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
 */

#include <errno.h>

#include "lua.h"
#include "lauxlib.h"
#include "lextlib.h"
#include "uj_capi_impl.h"

#include "lj_obj.h"
#include "uj_err.h"
#include "uj_dispatch.h"
#include "uj_str.h"
#include "lj_tab.h"
#include "uj_state.h"
#include "lj_debug.h"
#include "jit/lj_trace.h"
#include "uj_lib.h"
#include "lj_gc.h"

/* -- I/O error handling -------------------------------------------------- */

LUALIB_API int luaL_fileresult(lua_State *L, int stat, const char *fname)
{
	if (stat) {
		setboolV(L->top++, 1);
		return 1;
	}

	int en = errno; /* Lua API calls may change this value. */
	setnilV(L->top++);
	if (fname)
		lua_pushfstring(L, "%s: %s", fname, strerror(en));
	else
		lua_pushfstring(L, "%s", strerror(en));
	setintV(L->top++, en);
	lj_trace_abort(G(L));
	return 3;
}

/* -- Module registration ------------------------------------------------- */

LUALIB_API const char *luaL_findtable(lua_State *L, int idx, const char *fname,
				      int szhint)
{
	const char *e;
	lua_pushvalue(L, idx);
	do {
		e = strchr(fname, '.');
		if (e == NULL)
			e = fname + strlen(fname);
		lua_pushlstring(L, fname, (size_t)(e - fname));
		lua_rawget(L, -2);
		if (lua_isnil(L, -1)) { /* no such field? */
			lua_pop(L, 1); /* remove this nil */
			lua_createtable(L, 0, (*e == '.' ? 1 : szhint));
			lua_pushlstring(L, fname, (size_t)(e - fname));
			lua_pushvalue(L, -2);
			lua_settable(L, -4); /* set new table into field */
		} else if (!lua_istable(L, -1)) {
			lua_pop(L, 2); /* remove table and value */
			return fname; /* return problematic part of the name */
		}
		lua_remove(L, -2); /* remove previous table */
		fname = e + 1;
	} while (*e == '.');
	return NULL;
}

static int libsize(const luaL_Reg *l)
{
	int size = 0;
	for (; l->name; l++)
		size++;
	return size;
}

/*
 * According to the Lua 5.1 Reference Manual, this interface is deprecated
 * in favor of luaL_register. However, some software is reported to still use
 * it. So please leave it here in order not to break things.
 */
LUALIB_API void luaL_openlib(lua_State *L, const char *libname,
			     const luaL_Reg *l, int nup)
{
	if (libname) {
		/* Ensure library table exists and move it to below upvalues: */
		uj_lib_emplace(L, libname, libsize(l));
		lua_insert(L, -(nup + 1));
	}

	for (; l->name; l++) {
		for (int i = 0; i < nup; i++) /* copy upvalues to the top */
			lua_pushvalue(L, -nup);

		lua_pushcclosure(L, l->func, nup);
		lua_setfield(L, -(nup + 2), l->name);
	}

	lua_pop(L, nup); /* remove upvalues */
}

LUALIB_API void luaL_register(lua_State *L, const char *libname,
			      const luaL_Reg *l)
{
	if (libname)
		uj_lib_emplace(L, libname, libsize(l));

	for (; l->name; l++) {
		lua_pushcfunction(L, l->func);
		lua_setfield(L, -2, l->name);
	}
}

LUALIB_API const char *luaL_gsub(lua_State *L, const char *s, const char *p,
				 const char *r)
{
	const char *wild;
	size_t l = strlen(p);
	luaL_Buffer b = {0};
	luaL_buffinit(L, &b);
	while ((wild = strstr(s, p)) != NULL) {
		luaL_addlstring(&b, s, (size_t)(wild - s)); /* push prefix */
		luaL_addstring(&b, r); /* push replacement */
		s = wild + l; /* continue after `p' */
	}
	luaL_addstring(&b, s); /* push last suffix */
	luaL_pushresult(&b);
	return lua_tostring(L, -1);
}

/* -- Buffer handling ----------------------------------------------------- */

static size_t bufflen(const luaL_Buffer *B)
{
	return (size_t)(B->p - B->buffer);
}

static size_t bufffree(const luaL_Buffer *B)
{
	return (size_t)(LUAL_BUFFERSIZE - bufflen(B));
}

static int emptybuffer(luaL_Buffer *B)
{
	size_t l = bufflen(B);
	if (l == 0)
		return 0; /* put nothing on stack */
	lua_pushlstring(B->L, B->buffer, l);
	B->p = B->buffer;
	B->lvl++;
	return 1;
}

static void adjuststack(luaL_Buffer *B)
{
	if (B->lvl <= 1)
		return;

	lua_State *L = B->L;
	int toget = 1; /* number of levels to concat */
	size_t toplen = lua_strlen(L, -1);
	do {
		size_t l = lua_strlen(L, -(toget + 1));
		if (!(B->lvl - toget + 1 >= LUA_MINSTACK / 2 || toplen > l))
			break;
		toplen += l;
		toget++;
	} while (toget < B->lvl);
	lua_concat(L, toget);
	B->lvl = B->lvl - toget + 1;
}

LUALIB_API char *luaL_prepbuffer(luaL_Buffer *B)
{
	if (emptybuffer(B))
		adjuststack(B);
	return B->buffer;
}

LUALIB_API void luaL_addlstring(luaL_Buffer *B, const char *s, size_t l)
{
	while (l--)
		luaL_addchar(B, *s++);
}

LUALIB_API void luaL_addstring(luaL_Buffer *B, const char *s)
{
	luaL_addlstring(B, s, strlen(s));
}

LUALIB_API void luaL_pushresult(luaL_Buffer *B)
{
	emptybuffer(B);
	lua_concat(B->L, B->lvl);
	B->lvl = 1;
}

LUALIB_API void luaL_addvalue(luaL_Buffer *B)
{
	lua_State *L = B->L;
	size_t vl;
	const char *s = lua_tolstring(L, -1, &vl);
	if (vl <= bufffree(B)) { /* fit into buffer? */
		memcpy(B->p, s, vl); /* put it there */
		B->p += vl;
		lua_pop(L, 1); /* remove from stack */
	} else {
		if (emptybuffer(B))
			lua_insert(L, -2); /* put buffer before new value */
		B->lvl++; /* add new value into B stack */
		adjuststack(B);
	}
}

LUALIB_API void luaL_buffinit(lua_State *L, luaL_Buffer *B)
{
	B->L = L;
	B->p = B->buffer;
	B->lvl = 0;
}

/* -- Reference management ------------------------------------------------ */

#define FREELIST_REF 0

/* Convert a stack index to an absolute index. */
static int abs_index(lua_State *L, int i)
{
	return (i > 0 || i <= LUA_REGISTRYINDEX) ? i : lua_gettop(L) + i + 1;
}

LUALIB_API int luaL_ref(lua_State *L, int t)
{
	int ref;
	t = abs_index(L, t);
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1); /* remove from stack */
		return LUA_REFNIL; /* `nil' has a unique fixed reference */
	}
	lua_rawgeti(L, t, FREELIST_REF); /* get first free element */
	ref = (int)lua_tointeger(L, -1); /* ref = t[FREELIST_REF] */
	lua_pop(L, 1); /* remove it from stack */
	if (ref != 0) { /* any free element? */
		/* (t[FREELIST_REF] = t[ref]) */
		lua_rawgeti(L, t, ref);
		lua_rawseti(L, t, FREELIST_REF);
	} else { /* no free elements */
		ref = (int)lua_objlen(L, t);
		ref++; /* create new reference */
	}
	lua_rawseti(L, t, ref);
	return ref;
}

LUALIB_API void luaL_unref(lua_State *L, int t, int ref)
{
	if (ref < 0)
		return;

	t = abs_index(L, t);
	lua_rawgeti(L, t, FREELIST_REF);
	lua_rawseti(L, t, ref); /* t[ref] = t[FREELIST_REF] */
	lua_pushinteger(L, ref);
	lua_rawseti(L, t, FREELIST_REF); /* t[FREELIST_REF] = ref */
}

/* -- Default allocator and panic function -------------------------------- */

LUALIB_API lua_State *luaL_newstate(void)
{
	struct luae_Options opt = {0};

	return uj_state_newstate(&opt);
}

/* Number of frames for the leading and trailing part of a traceback. */
#define TRACEBACK_LEVELS1 12
#define TRACEBACK_LEVELS2 10

LUALIB_API void luaL_traceback(lua_State *L, lua_State *L1, const char *msg,
			       int level)
{
	int top = (int)(L->top - L->base);
	int lim = TRACEBACK_LEVELS1;
	lua_Debug ar;
	if (msg)
		lua_pushfstring(L, "%s\n", msg);
	lua_pushliteral(L, "stack traceback:");
	while (lua_getstack(L1, level++, &ar)) {
		GCfunc *fn;
		if (level > lim) {
			if (!lua_getstack(L1, level + TRACEBACK_LEVELS2, &ar)) {
				level--;
			} else {
				lua_pushliteral(L, "\n\t...");
				lua_getstack(L1, -10, &ar);
				level = ar.i_ci - TRACEBACK_LEVELS2;
			}
			lim = 2147483647;
			continue;
		}
		lua_getinfo(L1, "Snlf", &ar);
		fn = funcV(L1->top - 1);
		L1->top--;
		if (isffunc(fn) && !*ar.namewhat)
			lua_pushfstring(L, "\n\t[builtin#%d]:", fn->c.ffid);
		else
			lua_pushfstring(L, "\n\t%s:", ar.short_src);

		if (ar.currentline > 0)
			lua_pushfstring(L, "%d:", ar.currentline);

		if (*ar.namewhat) {
			lua_pushfstring(L, " in function " LUA_QS, ar.name);
		} else if (*ar.what == 'm') {
			lua_pushliteral(L, " in main chunk");
		} else if (*ar.what == 'C') {
			lua_pushfstring(L, " at %p", fn->c.f);
		} else {
			lua_pushfstring(L, " in function <%s:%d>", ar.short_src,
					ar.linedefined);
		}
		if ((int)(L->top - L->base) - top >= 15)
			lua_concat(L, (int)(L->top - L->base) - top);
	}
	lua_concat(L, (int)(L->top - L->base) - top);
}

LUALIB_API void luaL_checkstack(lua_State *L, int size, const char *msg)
{
	if (!lua_checkstack(L, size))
		uj_err_callerv(L, UJ_ERR_STKOVM, msg);
}

LUALIB_API void luaL_checktype(lua_State *L, int idx, int tt)
{
	if (lua_type(L, idx) != tt)
		uj_err_argt(L, idx, tt);
}

LUALIB_API void luaL_checkany(lua_State *L, int idx)
{
	if (uj_capi_index2adr(L, idx) == niltv(L))
		uj_err_arg(L, UJ_ERR_NOVAL, idx);
}

LUALIB_API lua_Number luaL_checknumber(lua_State *L, int idx)
{
	const TValue *o = uj_capi_index2adr(L, idx);
	TValue tmp;
	if (LJ_LIKELY(tvisnum(o)))
		return numV(o);
	else if (!(tvisstr(o) && uj_str_tonumtv(strV(o), &tmp)))
		uj_err_argt(L, idx, LUA_TNUMBER);
	return numV(&tmp);
}

LUALIB_API lua_Number luaL_optnumber(lua_State *L, int idx, lua_Number def)
{
	const TValue *o = uj_capi_index2adr(L, idx);
	if (LJ_UNLIKELY(tvisnil(o)))
		return def;
	return luaL_checknumber(L, idx);
}

LUALIB_API lua_Integer luaL_checkinteger(lua_State *L, int idx)
{
	return (lua_Integer)luaL_checknumber(L, idx);
}

LUALIB_API lua_Integer luaL_optinteger(lua_State *L, int idx, lua_Integer def)
{
	const TValue *o = uj_capi_index2adr(L, idx);
	if (LJ_UNLIKELY(tvisnil(o)))
		return def;
	return luaL_checkinteger(L, idx);
}

LUALIB_API const char *luaL_checklstring(lua_State *L, int idx, size_t *len)
{
	size_t strlen; /* avoid touching *len on error */
	const char *str = lua_tolstring(L, idx, &strlen);
	if (NULL == str)
		uj_err_argt(L, idx, LUA_TSTRING);

	if (NULL != len)
		*len = strlen;
	return str;
}

LUALIB_API const char *luaL_optlstring(lua_State *L, int idx, const char *def,
				       size_t *len)
{
	TValue *o = uj_capi_index2adr(L, idx);
	if (LJ_UNLIKELY(tvisnil(o))) {
		if (NULL != len)
			*len = def ? strlen(def) : 0;
		return def;
	}
	return luaL_checklstring(L, idx, len);
}

LUALIB_API int luaL_checkoption(lua_State *L, int idx, const char *def,
				const char *const lst[])
{
	ptrdiff_t i;
	const char *s = lua_tolstring(L, idx, NULL);
	if (s == NULL) {
		s = def;
		if (s == NULL)
			uj_err_argt(L, idx, LUA_TSTRING);
	}
	for (i = 0; lst[i]; i++)
		if (strcmp(lst[i], s) == 0)
			return (int)i;
	uj_err_argv(L, UJ_ERR_INVOPTM, idx, s);
}

LUALIB_API int luaL_newmetatable(lua_State *L, const char *tname)
{
	GCtab *regt = tabV(registry(L));
	TValue *tv = lj_tab_setstr(L, regt, uj_str_newz(L, tname));
	if (tvisnil(tv)) {
		GCtab *mt = lj_tab_new(L, 0, 1);
		settabV(L, tv, mt);
		settabV(L, L->top++, mt);
		lj_gc_anybarriert(L, regt);
		return 1;
	} else {
		copyTV(L, L->top++, tv);
		return 0;
	}
}

LUALIB_API int luaL_getmetafield(lua_State *L, int idx, const char *field)
{
	if (lua_getmetatable(L, idx)) {
		const TValue *tv =
			lj_tab_getstr(tabV(L->top - 1), uj_str_newz(L, field));
		if (tv && !tvisnil(tv)) {
			copyTV(L, L->top - 1, tv);
			return 1;
		}
		L->top--;
	}
	return 0;
}

LUALIB_API void *luaL_checkudata(lua_State *L, int idx, const char *tname)
{
	const TValue *o = uj_capi_index2adr(L, idx);
	if (tvisudata(o)) {
		GCudata *ud = udataV(o);
		const TValue *tv =
			lj_tab_getstr(tabV(registry(L)), uj_str_newz(L, tname));
		if (tv && tvistab(tv) && tabV(tv) == ud->metatable)
			return uddata(ud);
	}
	uj_err_argtype(L, idx, tname);
	return NULL; /* unreachable */
}

LUALIB_API int luaL_callmeta(lua_State *L, int idx, const char *field)
{
	if (!luaL_getmetafield(L, idx, field))
		return 0;
	TValue *base = L->top--;
	copyTV(L, base, uj_capi_index2adr(L, idx));
	L->top = base + 1;
	lj_vm_call(L, base, 1 + 1);
	return 1;
}

struct FileReaderCtx {
	FILE *fp;
	char buf[LUAL_BUFFERSIZE];
};

static const char *reader_file(lua_State *L, void *ud, size_t *size)
{
	struct FileReaderCtx *ctx = (struct FileReaderCtx *)ud;
	UNUSED(L);
	if (feof(ctx->fp))
		return NULL;
	*size = fread(ctx->buf, 1, sizeof(ctx->buf), ctx->fp);
	return *size > 0 ? ctx->buf : NULL;
}

LUALIB_API int luaL_loadfilex(lua_State *L, const char *filename,
			      const char *mode)
{
	struct FileReaderCtx ctx;
	int status;
	const char *chunkname;
	if (filename) {
		ctx.fp = fopen(filename, "rb");
		if (ctx.fp == NULL) {
			lua_pushfstring(L, "cannot open %s: %s", filename,
					strerror(errno));
			return LUA_ERRFILE;
		}
		chunkname = lua_pushfstring(L, "@%s", filename);
	} else {
		ctx.fp = stdin;
		chunkname = "=stdin";
	}
	status = lua_loadx(L, reader_file, &ctx, chunkname, mode);
	if (ferror(ctx.fp)) {
		L->top -= filename ? 2 : 1;
		lua_pushfstring(L, "cannot read %s: %s", chunkname + 1,
				strerror(errno));
		if (filename)
			fclose(ctx.fp);
		return LUA_ERRFILE;
	}
	if (filename) {
		lua_remove(L, -2);
		fclose(ctx.fp);
	}
	return status;
}

LUALIB_API int luaL_loadfile(lua_State *L, const char *filename)
{
	return luaL_loadfilex(L, filename, NULL);
}

struct StringReaderCtx {
	const char *str;
	size_t size;
};

static const char *reader_string(lua_State *L, void *ud, size_t *size)
{
	struct StringReaderCtx *ctx = (struct StringReaderCtx *)ud;
	UNUSED(L);
	if (ctx->size == 0)
		return NULL;
	*size = ctx->size;
	ctx->size = 0;
	return ctx->str;
}

LUALIB_API int luaL_loadbufferx(lua_State *L, const char *buf, size_t size,
				const char *name, const char *mode)
{
	struct StringReaderCtx ctx;
	ctx.str = buf;
	ctx.size = size;
	return lua_loadx(L, reader_string, &ctx, name, mode);
}

LUALIB_API int luaL_loadbuffer(lua_State *L, const char *buf, size_t size,
			       const char *name)
{
	return luaL_loadbufferx(L, buf, size, name, NULL);
}

LUALIB_API int luaL_loadstring(lua_State *L, const char *s)
{
	return luaL_loadbuffer(L, s, strlen(s), s);
}

LUALIB_API int luaL_argerror(lua_State *L, int narg, const char *msg)
{
	uj_err_msg_arg(L, msg, narg);
	return 0; /* unreachable */
}

LUALIB_API int luaL_typerror(lua_State *L, int narg, const char *xname)
{
	uj_err_argtype(L, narg, xname);
	return 0; /* unreachable */
}

LUALIB_API void luaL_where(lua_State *L, int level)
{
	int size;
	const TValue *frame = lj_debug_frame(L, level, &size);
	lj_debug_addloc(L, "", frame, size ? frame + size : NULL);
}

LUALIB_API int luaL_error(lua_State *L, const char *fmt, ...)
{
	const char *msg;
	va_list argp;
	va_start(argp, fmt);
	msg = uj_str_pushvf(L, fmt, argp);
	va_end(argp);
	uj_err_msg_caller(L, msg);
	return 0; /* unreachable */
}
