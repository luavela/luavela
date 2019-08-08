/*
 * FFI C library loader.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "lj_obj.h"

#if LJ_HASFFI

#include "lj_gc.h"
#include "uj_err.h"
#include "lj_tab.h"
#include "uj_str.h"
#include "uj_udata.h"
#include "ffi/lj_ctype.h"
#include "ffi/lj_cconv.h"
#include "ffi/lj_cdata.h"
#include "ffi/lj_clib.h"

/* -- OS-specific functions ----------------------------------------------- */

#include <dlfcn.h>
#include <stdio.h>

#if defined(RTLD_DEFAULT)
#define CLIB_DEFHANDLE  RTLD_DEFAULT
#else
#define CLIB_DEFHANDLE  NULL
#endif

LJ_NORET LJ_NOINLINE static void clib_error_(lua_State *L)
{
  uj_err_msg_caller(L, dlerror());
}

#define clib_error(L, fmt, name)        clib_error_(L)

#define CLIB_SOPREFIX   "lib"
#define CLIB_SOEXT      "%s.so"

static const char *clib_extname(lua_State *L, const char *name)
{
  if (!strchr(name, '/')) {
    if (!strchr(name, '.')) {
      name = uj_str_pushf(L, CLIB_SOEXT, name);
      L->top--;
    }
    if (!(name[0] == CLIB_SOPREFIX[0] && name[1] == CLIB_SOPREFIX[1] &&
          name[2] == CLIB_SOPREFIX[2])) {
      name = uj_str_pushf(L, CLIB_SOPREFIX "%s", name);
      L->top--;
    }
  }
  return name;
}

/* Check for a recognized ld script line. */
static const char *clib_check_lds(lua_State *L, const char *buf)
{
  char *p, *e;
  if ((!strncmp(buf, "GROUP", 5) || !strncmp(buf, "INPUT", 5)) &&
      (p = strchr(buf, '('))) {
    while (*++p == ' ') ;
    for (e = p; *e && *e != ' ' && *e != ')'; e++) ;
    return strdata(uj_str_new(L, p, e-p));
  }
  return NULL;
}

/* Quick and dirty solution to resolve shared library name from ld script. */
static const char *clib_resolve_lds(lua_State *L, const char *name)
{
  FILE *fp = fopen(name, "r");
  const char *p = NULL;
  if (fp) {
    char buf[256];
    if (fgets(buf, sizeof(buf), fp)) {
      if (!strncmp(buf, "/* GNU ld script", 16)) {  /* ld script magic? */
        while (fgets(buf, sizeof(buf), fp)) {  /* Check all lines. */
          p = clib_check_lds(L, buf);
          if (p) break;
        }
      } else {  /* Otherwise check only the first line. */
        p = clib_check_lds(L, buf);
      }
    }
    fclose(fp);
  }
  return p;
}

static void *clib_loadlib(lua_State *L, const char *name, int global)
{
  void *h = dlopen(clib_extname(L, name),
                   RTLD_LAZY | (global?RTLD_GLOBAL:RTLD_LOCAL));
  if (!h) {
    const char *e, *err = dlerror();
    if (*err == '/' && (e = strchr(err, ':')) &&
        (name = clib_resolve_lds(L, strdata(uj_str_new(L, err, e-err))))) {
      h = dlopen(name, RTLD_LAZY | (global?RTLD_GLOBAL:RTLD_LOCAL));
      if (h) return h;
      err = dlerror();
    }
    uj_err_msg_caller(L, err);
  }
  return h;
}

static void clib_unloadlib(CLibrary *cl)
{
  if (cl->handle && cl->handle != CLIB_DEFHANDLE)
    dlclose(cl->handle);
}

static void *clib_getsym(CLibrary *cl, const char *name)
{
  void *p = dlsym(cl->handle, name);
  return p;
}

/* -- C library indexing -------------------------------------------------- */

/* Get redirected or mangled external symbol. */
static const char *clib_extsym(CTState *cts, CType *ct, GCstr *name)
{
  if (ct->sib) {
    CType *ctf = ctype_get(cts, ct->sib);
    if (ctype_isxattrib(ctf->info, CTA_REDIR))
      return strdata(ctf->name);
  }
  return strdata(name);
}

/* Index a C library by name. */
TValue *lj_clib_index(lua_State *L, CLibrary *cl, GCstr *name)
{
  TValue *tv = lj_tab_setstr(L, cl->cache, name);
  if (LJ_UNLIKELY(tvisnil(tv))) {
    CTState *cts = ctype_cts(L);
    CType *ct;
    CTypeID id = lj_ctype_getname(cts, &ct, name, CLNS_INDEX);
    if (!id)
      uj_err_callerv(L, UJ_ERR_FFI_NODECL, strdata(name));
    if (ctype_isconstval(ct->info)) {
      CType *ctt = ctype_child(cts, ct);
      lua_assert(ctype_isinteger(ctt->info) && ctt->size <= 4);
      if ((ctt->info & CTF_UNSIGNED) && (int32_t)ct->size < 0)
        setnumV(tv, (lua_Number)(uint32_t)ct->size);
      else
        setintV(tv, (int32_t)ct->size);
    } else {
      const char *sym = clib_extsym(cts, ct, name);
      void *p = clib_getsym(cl, sym);
      GCcdata *cd;
      lua_assert(ctype_isfunc(ct->info) || ctype_isextern(ct->info));
      if (!p)
        clib_error(L, "cannot resolve symbol " LUA_QS ": %s", sym);
      cd = lj_cdata_new(cts, id, CTSIZE_PTR);
      *(void **)cdataptr(cd) = p;
      setcdataV(L, tv, cd);
    }
  }
  return tv;
}

/* -- C library management ------------------------------------------------ */

/* Create a new CLibrary object and push it on the stack. */
static CLibrary *clib_new(lua_State *L, GCtab *mt)
{
  GCtab *t = lj_tab_new(L, 0, 0);
  GCudata *ud = uj_udata_new(L, sizeof(CLibrary), t);
  CLibrary *cl = (CLibrary *)uddata(ud);
  cl->cache = t;
  ud->udtype = UDTYPE_FFI_CLIB;
  /* NOBARRIER: The GCudata is new (marked white). */
  ud->metatable = mt;
  setudataV(L, L->top++, ud);
  return cl;
}

/* Load a C library. */
void lj_clib_load(lua_State *L, GCtab *mt, GCstr *name, int global)
{
  void *handle = clib_loadlib(L, strdata(name), global);
  CLibrary *cl = clib_new(L, mt);
  cl->handle = handle;
}

/* Unload a C library. */
void lj_clib_unload(CLibrary *cl)
{
  clib_unloadlib(cl);
  cl->handle = NULL;
}

/* Create the default C library object. */
void lj_clib_default(lua_State *L, GCtab *mt)
{
  CLibrary *cl = clib_new(L, mt);
  cl->handle = CLIB_DEFHANDLE;
}

#endif
