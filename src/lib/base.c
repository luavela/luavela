/*
 * Base and coroutine library.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 *
 * Major portions taken verbatim or adapted from the Lua interpreter.
 * Copyright (C) 1994-2011 Lua.org, PUC-Rio. See Copyright Notice in lua.h
 */

#include <stdio.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "lj_obj.h"
#include "uj_mem.h"
#include "lj_gc.h"
#include "uj_err.h"
#include "uj_throw.h"
#include "uj_errmsg.h"
#include "lj_debug.h"
#include "uj_str.h"
#include "lj_tab.h"
#include "uj_meta.h"
#include "uj_state.h"
#if LJ_HASFFI
#include "ffi/lj_ctype.h"
#include "ffi/lj_cconv.h"
#endif
#include "lj_bc.h"
#include "uj_ff.h"
#include "uj_dispatch.h"
#include "utils/lj_char.h"
#include "uj_lib.h"

/* -- Base library: checks ------------------------------------------------ */

#define LJLIB_MODULE_base

LJLIB_ASM(assert)               LJLIB_REC(.)
{
  GCstr *s;
  uj_lib_checkany(L, 1);
  s = uj_lib_optstr(L, 2);
  if (s)
    uj_err_msg_caller(L, strdata(s));
  else
    uj_err_caller(L, UJ_ERR_ASSERT);
  return FFH_UNREACHABLE;
}

/* ORDER LJ_T */
LJLIB_PUSH("nil")
LJLIB_PUSH("boolean")
LJLIB_PUSH(top-1)  /* boolean */
LJLIB_PUSH("userdata")
LJLIB_PUSH("string")
LJLIB_PUSH("upval")
LJLIB_PUSH("thread")
LJLIB_PUSH("proto")
LJLIB_PUSH("function")
LJLIB_PUSH("trace")
LJLIB_PUSH("cdata")
LJLIB_PUSH("table")
LJLIB_PUSH(top-9)  /* userdata */
LJLIB_PUSH("number")
LJLIB_ASM_(type)                LJLIB_REC(.)
/* Recycle the uj_lib_checkany(L, 1) from assert. */

/* -- Base library: iterators --------------------------------------------- */

/* This solves a circular dependency problem -- change FF_next_N as needed. */
LJ_STATIC_ASSERT((int)FF_next == FF_next_N);

LJLIB_ASM(next)  LJLIB_REC(.)
{
  uj_lib_checktab(L, 1);
  return FFH_UNREACHABLE;
}

#if LJ_52 || LJ_HASFFI
static int ffh_pairs(lua_State *L, enum MMS mm)
{
  TValue *o = uj_lib_checkany(L, 1);
  const TValue *mo = uj_meta_lookup(L, o, mm);
  if ((LJ_52 || tviscdata(o)) && !tvisnil(mo)) {
    L->top = o+1;  /* Only keep one argument. */
    copyTV(L, L->base-1, mo);  /* Replace callable. */
    return FFH_TAILCALL;
  } else {
    if (!tvistab(o)) uj_err_argt(L, 1, LUA_TTABLE);
    setfuncV(L, o-1, funcV(uj_lib_upvalue(L, 1)));
    if (mm == MM_pairs) setnilV(o+1); else setintV(o+1, 0);
    return FFH_RES(3);
  }
}
#else
#define ffh_pairs(L, mm)        (uj_lib_checktab(L, 1), FFH_UNREACHABLE)
#endif

LJLIB_PUSH(lastcl)
LJLIB_ASM(pairs)     LJLIB_REC(.)
{
  return ffh_pairs(L, MM_pairs);
}

LJLIB_NOREGUV LJLIB_ASM(ipairs_aux)     LJLIB_REC(.)
{
  uj_lib_checktab(L, 1);
  uj_lib_checkint(L, 2);
  return FFH_UNREACHABLE;
}

LJLIB_PUSH(lastcl)
LJLIB_ASM(ipairs)               LJLIB_REC(.)
{
  return ffh_pairs(L, MM_ipairs);
}

/* -- Base library: getters and setters ----------------------------------- */

LJLIB_ASM_(getmetatable)        LJLIB_REC(.)
/* Recycle the uj_lib_checkany(L, 1) from assert. */

LJLIB_ASM(setmetatable)         LJLIB_REC(.)
{
  GCtab *t = uj_lib_checktab(L, 1);
  GCtab *mt = uj_lib_checktabornil(L, 2);
  if (LJ_UNLIKELY(uj_obj_is_immutable(obj2gco(t)))) {
    uj_err(L, UJ_ERR_IMMUT_MODIF);
  }
  if (!tvisnil(uj_meta_lookup(L, L->base, MM_metatable))) {
    uj_err_caller(L, UJ_ERR_PROTMT);
  }
  t->metatable = mt;
  if (mt) {
    lj_gc_objbarriert(L, t, mt);
  }
  settabV(L, L->base-1, t);
  return FFH_RES(1);
}

LJLIB_CF(getfenv)		LJLIB_REC(.)
{
  GCfunc *fn;
  const TValue *o = L->base;
  if (!(o < L->top && tvisfunc(o))) {
    int level = uj_lib_optint(L, 1, 1);
    o = lj_debug_frame(L, level, &level);
    if (o == NULL)
      uj_err_arg(L, UJ_ERR_INVLVL, 1);
  }
  fn = &gcval(o)->fn;
  settabV(L, L->top++, isluafunc(fn) ? fn->l.env : L->env);
  return 1;
}

LJLIB_CF(setfenv)
{
  GCfunc *fn;
  GCtab *t = uj_lib_checktab(L, 2);
  const TValue *o = L->base;
  if (!(o < L->top && tvisfunc(o))) {
    int level = uj_lib_checkint(L, 1);
    if (level == 0) {
      /* NOBARRIER: A thread (i.e. L) is never black. */
      L->env = t;
      return 0;
    }
    o = lj_debug_frame(L, level, &level);
    if (o == NULL)
      uj_err_arg(L, UJ_ERR_INVLVL, 1);
  }
  fn = &gcval(o)->fn;
  if (!isluafunc(fn))
    uj_err_caller(L, UJ_ERR_SETFENV);
  fn->l.env = t;
  lj_gc_objbarrier(L, obj2gco(fn), t);
  setfuncV(L, L->top++, fn);
  return 1;
}

LJLIB_ASM(rawget)               LJLIB_REC(.)
{
  uj_lib_checktab(L, 1);
  uj_lib_checkany(L, 2);
  return FFH_UNREACHABLE;
}

LJLIB_CF(rawset)                LJLIB_REC(.)
{
  uj_lib_checktab(L, 1);
  uj_lib_checkany(L, 2);
  L->top = 1+uj_lib_checkany(L, 3);
  lua_rawset(L, 1);
  return 1;
}

LJLIB_CF(rawequal)              LJLIB_REC(.)
{
  const TValue *o1 = uj_lib_checkany(L, 1);
  const TValue *o2 = uj_lib_checkany(L, 2);
  setboolV(L->top-1, uj_obj_equal(o1, o2));
  return 1;
}

#if LJ_52
LJLIB_CF(rawlen)                LJLIB_REC(.)
{
  const TValue *o = L->base;
  int32_t len;
  if (L->top > o && tvisstr(o))
    len = (int32_t)strV(o)->len;
  else
    len = (int32_t)lj_tab_len(uj_lib_checktab(L, 1));
  setintV(L->top-1, len);
  return 1;
}
#endif

LJLIB_CF(unpack)
{
  GCtab *t = uj_lib_checktab(L, 1);
  int32_t n, i = uj_lib_optint(L, 2, 1);
  int32_t e = (L->base+3-1 < L->top && !tvisnil(L->base+3-1)) ?
              uj_lib_checkint(L, 3) : (int32_t)lj_tab_len(t);
  if (i > e) return 0;
  n = e - i + 1;
  if (n <= 0 || !lua_checkstack(L, n))
    uj_err_caller(L, UJ_ERR_UNPACK);
  do {
    const TValue *tv = lj_tab_getint(t, i);
    if (tv) {
      copyTV(L, L->top++, tv);
    } else {
      setnilV(L->top++);
    }
  } while (i++ < e);
  return n;
}

LJLIB_CF(select)                LJLIB_REC(.)
{
  int32_t n = (int32_t)(L->top - L->base);
  if (n >= 1 && tvisstr(L->base) && *strVdata(L->base) == '#') {
    setintV(L->top-1, n-1);
    return 1;
  } else {
    int32_t i = uj_lib_checkint(L, 1);
    if (i < 0) i = n + i; else if (i > n) i = n;
    if (i < 1)
      uj_err_arg(L, UJ_ERR_IDXRNG, 1);
    return n - i;
  }
}

/* -- Base library: conversions ------------------------------------------- */

LJLIB_ASM(tonumber)             LJLIB_REC(.)
{
  int32_t base = uj_lib_optint(L, 2, 10);
  if (base == 10) {
    TValue *o = uj_lib_checkany(L, 1);
    if (uj_str_tonumber(o)) {
      copyTV(L, L->base-1, o);
      return FFH_RES(1);
    }
#if LJ_HASFFI
    if (tviscdata(o)) {
      CTState *cts = ctype_cts(L);
      CType *ct = lj_ctype_rawref(cts, cdataV(o)->ctypeid);
      if (ctype_isenum(ct->info)) ct = ctype_child(cts, ct);
      if (ctype_isnum(ct->info) || ctype_iscomplex(ct->info)) {
        TValue *dst = L->base-1;
        lj_cconv_ct_tv(cts, ctype_get(cts, CTID_DOUBLE),
                       (uint8_t *)&dst->n, o, 0);
        settag(dst, LJ_TNUMX);
        return FFH_RES(1);
      }
    }
#endif
  } else {
    const char *p = strdata(uj_lib_checkstr(L, 1));
    char *ep;
    unsigned long ul;
    if (base < 2 || base > 36)
      uj_err_arg(L, UJ_ERR_BASERNG, 2);
    ul = strtoul(p, &ep, base);
    if (p != ep) {
      while (lj_char_isspace((unsigned char)(*ep))) ep++;
      if (*ep == '\0') {
        setnumV(L->base-1, (lua_Number)ul);
        return FFH_RES(1);
      }
    }
  }
  setnilV(L->base-1);
  return FFH_RES(1);
}

LJLIB_PUSH("nil")
LJLIB_PUSH("false")
LJLIB_PUSH("true")
LJLIB_ASM(tostring)             LJLIB_REC(.)
{
  TValue *o = uj_lib_checkany(L, 1);
  const TValue *mo;
  L->top = o + uj_mm_narg[MM_tostring];  /* Only keep one argument. */
  mo = uj_meta_lookup(L, o, MM_tostring);
  if (!tvisnil(mo)) {
    copyTV(L, L->base-1, mo);  /* Replace callable. */
    return FFH_TAILCALL;
  } else {
    GCstr *s;
    if (tvisnum(o)) {
      s = uj_str_fromnumber(L, o->n);
    } else if (tvispri(o)) {
      s = strV(uj_lib_upvalue(L, -(int32_t)gettag(o)));
    } else {
      if (tvisfunc(o) && isffunc(funcV(o)))
        lua_pushfstring(L, "function: builtin#%d", funcV(o)->c.ffid);
      else
        lua_pushfstring(L, "%s: %p", lj_typename(o), lua_topointer(L, 1));
      /* Note: lua_pushfstring calls the GC which may invalidate o. */
      s = strV(L->top-1);
    }
    setstrV(L, L->base-1, s);
    return FFH_RES(1);
  }
}

/* -- Base library: throw and catch errors -------------------------------- */

LJLIB_CF(error)
{
  int32_t level = uj_lib_optint(L, 2, 1);
  lua_settop(L, 1);
  if (lua_isstring(L, 1) && level > 0) {
    luaL_where(L, level);
    lua_pushvalue(L, 1);
    lua_concat(L, 2);
  }
  return lua_error(L);
}

LJLIB_ASM(pcall)                LJLIB_REC(.)
{
  uj_lib_checkany(L, 1);
  uj_lib_checkfunc(L, 2);  /* For xpcall only. */
  return FFH_UNREACHABLE;
}
LJLIB_ASM_(xpcall)              LJLIB_REC(.)

/* -- Base library: load Lua code ----------------------------------------- */

static int load_aux(lua_State *L, int status, int envarg)
{
  if (status == 0) {
    if (tvistab(L->base+envarg-1)) {
      GCfunc *fn = funcV(L->top-1);
      GCtab *t = tabV(L->base+envarg-1);
      fn->c.env = t;
      lj_gc_objbarrier(L, fn, t);
    }
    return 1;
  } else {
    setnilV(L->top-2);
    return 2;
  }
}

LJLIB_CF(loadfile)
{
  GCstr *fname = uj_lib_optstr(L, 1);
  GCstr *mode = uj_lib_optstr(L, 2);
  int status;
  lua_settop(L, 3);  /* Ensure env arg exists. */
  status = luaL_loadfilex(L, fname ? strdata(fname) : NULL,
                          mode ? strdata(mode) : NULL);
  return load_aux(L, status, 3);
}

static const char *reader_func(lua_State *L, void *ud, size_t *size)
{
  UNUSED(ud);
  luaL_checkstack(L, 2, "too many nested functions");
  copyTV(L, L->top++, L->base);
  lua_call(L, 0, 1);  /* Call user-supplied function. */
  L->top--;
  if (tvisnil(L->top)) {
    *size = 0;
    return NULL;
  } else if (tvisstr(L->top) || tvisnum(L->top)) {
    copyTV(L, L->base+4, L->top);  /* Anchor string in reserved stack slot. */
    return lua_tolstring(L, 5, size);
  } else {
    uj_err_caller(L, UJ_ERR_RDRSTR);
    return NULL;
  }
}

LJLIB_CF(load)
{
  GCstr *name = uj_lib_optstr(L, 2);
  GCstr *mode = uj_lib_optstr(L, 3);
  int status;
  if (L->base < L->top && (tvisstr(L->base) || tvisnum(L->base))) {
    GCstr *s = uj_lib_checkstr(L, 1);
    lua_settop(L, 4);  /* Ensure env arg exists. */
    status = luaL_loadbufferx(L, strdata(s), s->len, strdata(name ? name : s),
                              mode ? strdata(mode) : NULL);
  } else {
    uj_lib_checkfunc(L, 1);
    lua_settop(L, 5);  /* Reserve a slot for the string from the reader. */
    status = lua_loadx(L, reader_func, NULL, name ? strdata(name) : "=(load)",
                       mode ? strdata(mode) : NULL);
  }
  return load_aux(L, status, 4);
}

LJLIB_CF(loadstring)
{
  return lj_cf_load(L);
}

LJLIB_CF(dofile)
{
  GCstr *fname = uj_lib_optstr(L, 1);
  setnilV(L->top);
  L->top = L->base+1;
  if (luaL_loadfile(L, fname ? strdata(fname) : NULL) != 0)
    lua_error(L);
  lua_call(L, 0, LUA_MULTRET);
  return (int)(L->top - L->base) - 1;
}

/* -- Base library: GC control -------------------------------------------- */

LJLIB_CF(gcinfo)
{
  setintV(L->top++, (uj_mem_total(MEM(L)) >> 10));
  return 1;
}

LJLIB_CF(collectgarbage)
{
  int opt = uj_lib_checkopt(L, 1, LUA_GCCOLLECT,  /* ORDER LUA_GC* */
    "\4stop\7restart\7collect\5count\1\377\4step\10setpause\12setstepmul");
  int32_t data = uj_lib_optint(L, 2, 0);
  if (opt == LUA_GCCOUNT) {
    setnumV(L->top, ((lua_Number)uj_mem_total(MEM(L))) / 1024.0);
  } else {
    int res = lua_gc(L, opt, data);
    if (opt == LUA_GCSTEP)
      setboolV(L->top, res);
    else
      setintV(L->top, res);
  }
  L->top++;
  return 1;
}

/* -- Base library: miscellaneous functions ------------------------------- */

LJLIB_PUSH(top-2)  /* Upvalue holds weak table. */
LJLIB_CF(newproxy)
{
  lua_settop(L, 1);
  lua_newuserdata(L, 0);
  if (lua_toboolean(L, 1) == 0) {  /* newproxy(): without metatable. */
    return 1;
  } else if (lua_isboolean(L, 1)) {  /* newproxy(true): with metatable. */
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_pushboolean(L, 1);
    lua_rawset(L, lua_upvalueindex(1));  /* Remember mt in weak table. */
  } else {  /* newproxy(proxy): inherit metatable. */
    int validproxy = 0;
    if (lua_getmetatable(L, 1)) {
      lua_rawget(L, lua_upvalueindex(1));
      validproxy = lua_toboolean(L, -1);
      lua_pop(L, 1);
    }
    if (!validproxy)
      uj_err_arg(L, UJ_ERR_NOPROXY, 1);
    lua_getmetatable(L, 1);
  }
  lua_setmetatable(L, 2);
  return 1;
}

LJLIB_PUSH("tostring")
LJLIB_CF(print)
{
  ptrdiff_t i, nargs = L->top - L->base;
  const TValue *tv = lj_tab_getstr(L->env, strV(uj_lib_upvalue(L, 1)));
  int shortcut;
  if (tv && !tvisnil(tv)) {
    copyTV(L, L->top++, tv);
  } else {
    setstrV(L, L->top++, strV(uj_lib_upvalue(L, 1)));
    lua_gettable(L, LUA_GLOBALSINDEX);
    tv = L->top-1;
  }
  shortcut = (tvisfunc(tv) && funcV(tv)->c.ffid == FF_tostring);
  for (i = 0; i < nargs; i++) {
    const char *str;
    size_t size;
    const TValue *o = &L->base[i];
    if (shortcut && tvisstr(o)) {
      str = strVdata(o);
      size = strV(o)->len;
    } else if (shortcut && tvisnum(o)) {
      char buf[UJ_CSTR_NUMBUF];
      size = uj_cstr_fromnum(buf, numV(o));
      str = buf;
    } else {
      copyTV(L, L->top+1, o);
      copyTV(L, L->top, L->top-1);
      L->top += 2;
      lua_call(L, 1, 1);
      str = lua_tolstring(L, -1, &size);
      if (!str)
        uj_err_caller(L, UJ_ERR_PRTOSTR);
      L->top--;
    }
    if (i)
      putchar('\t');
    fwrite(str, 1, size, stdout);
  }
  putchar('\n');
  return 0;
}

LJLIB_PUSH(top-3)
LJLIB_SET(_VERSION)

#include "lj_libdef.h"

/* -- Coroutine library --------------------------------------------------- */

#define LJLIB_MODULE_coroutine

LJLIB_CF(coroutine_status)
{
  const char *s;
  lua_State *co;
  if (!(L->top > L->base && tvisthread(L->base)))
    uj_err_arg(L, UJ_ERR_NOCORO, 1);
  co = threadV(L->base);
  if (co == L) s = "running";
  else if (co->status == LUA_YIELD) s = "suspended";
  else if (co->status != 0) s = "dead";
  else if (co->base > co->stack + 1) s = "normal";
  else if (co->top == co->base) s = "dead";
  else s = "suspended";
  lua_pushstring(L, s);
  return 1;
}

LJLIB_CF(coroutine_running)
{
  /* We enforce unconditional Lua 5.1-compatible behaviour, original
   * implementation commented out below
   */
  if (lua_pushthread(L))
    setnilV(L->top++);
  return 1;
/*
#if LJ_52
  int ismain = lua_pushthread(L);
  setboolV(L->top++, ismain);
  return 2;
#else
  if (lua_pushthread(L))
    setnilV(L->top++);
  return 1;
#endif
*/
}

LJLIB_CF(coroutine_create)
{
  lua_State *L1;
  if (!(L->base < L->top && tvisfunc(L->base)))
    uj_err_argt(L, 1, LUA_TFUNCTION);
  L1 = lua_newthread(L);
  setfuncV(L, L1->top++, funcV(L->base));
  return 1;
}

LJLIB_ASM(coroutine_yield)
{
  uj_err_caller(L, UJ_ERR_CYIELD);
  return FFH_UNREACHABLE;
}

static int ffh_resume(lua_State *L, lua_State *co, int wrap)
{
  if (uj_state_has_timeout(co)) {
    uj_err_caller(L, UJ_ERR_COTICKS);
    return FFH_UNREACHABLE;
  }
  if (co->cframe != NULL || co->status > LUA_YIELD ||
      (co->status == 0 && co->top == co->base)) {
    enum err_msg em = co->cframe ? UJ_ERR_CORUN : UJ_ERR_CODEAD;
    if (wrap) uj_err_caller(L, em);
    setboolV(L->base-1, 0);
    setstrV(L, L->base, uj_errmsg_str(L, em));
    return FFH_RES(2);
  }
  uj_state_stack_grow(co, (size_t)(L->top - L->base));
  return FFH_RETRY;
}

LJLIB_ASM(coroutine_resume)
{
  if (!(L->top > L->base && tvisthread(L->base)))
    uj_err_arg(L, UJ_ERR_NOCORO, 1);
  return ffh_resume(L, threadV(L->base), 0);
}

LJLIB_NOREG LJLIB_ASM(coroutine_wrap_aux)
{
  return ffh_resume(L, threadV(uj_lib_upvalue(L, 1)), 1);
}

/* Inline declarations. */
void lj_ff_coroutine_wrap_aux(void);
LJ_NORET void lj_ffh_coroutine_wrap_err(lua_State *L, lua_State *co);

/* Error handler, called from assembler VM. */
void lj_ffh_coroutine_wrap_err(lua_State *L, lua_State *co)
{
  co->top--; copyTV(L, L->top, co->top); L->top++;
  if (tvisstr(L->top-1))
    uj_err_msg_caller(L, strVdata(L->top-1));
  else
    uj_throw_run(L);
}

/* Forward declaration. */
static void setpc_wrap_aux(lua_State *L, GCfunc *fn);

LJLIB_CF(coroutine_wrap)
{
  lj_cf_coroutine_create(L);
  uj_lib_pushcc(L, lj_ffh_coroutine_wrap_aux, FF_coroutine_wrap_aux, 1);
  setpc_wrap_aux(L, funcV(L->top-1));
  return 1;
}

#include "lj_libdef.h"

/* Fix the PC of wrap_aux. Really ugly workaround. */
static void setpc_wrap_aux(lua_State *L, GCfunc *fn)
{
  fn->c.pc = &L2GG(L)->bcff[lj_lib_init_coroutine[1]+2];
}

/* ------------------------------------------------------------------------ */

static void newproxy_weaktable(lua_State *L)
{
  /* NOBARRIER: The table is new (marked white). */
  GCtab *t = lj_tab_new(L, 0, 1);
  settabV(L, L->top++, t);
  t->metatable = t;
  setstrV(L, lj_tab_setstr(L, t, uj_str_newz(L, "__mode")),
            uj_str_newz(L, "kv"));
  t->nomm = (uint8_t)(~(1u<<MM_mode));
}

LUALIB_API int luaopen_base(lua_State *L)
{
  /* NOBARRIER: Table and value are the same. */
  GCtab *env = L->env;
  settabV(L, lj_tab_setstr(L, env, uj_str_newz(L, "_G")), env);
  lua_pushliteral(L, LUA_VERSION);  /* top-3. */
  newproxy_weaktable(L);  /* top-2. */
  LJ_LIB_REG(L, "_G", base);
  LJ_LIB_REG(L, LUA_COLIBNAME, coroutine);
  return 2;
}

