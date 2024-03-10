/*
 * FFI C library loader.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _LJ_CLIB_H
#define _LJ_CLIB_H

#include "lj_obj.h"

#if LJ_HASFFI

/* Namespace for C library indexing. */
#define CLNS_INDEX      ((1u<<CT_FUNC)|(1u<<CT_EXTERN)|(1u<<CT_CONSTVAL))

/* C library namespace. */
typedef struct CLibrary {
  void *handle;         /* Opaque handle for dynamic library loader. */
  GCtab *cache;         /* Cache for resolved symbols. Anchored in ud->env. */
} CLibrary;

TValue *lj_clib_index(lua_State *L, CLibrary *cl, GCstr *name);
void lj_clib_load(lua_State *L, GCtab *mt, GCstr *name, int global);
void lj_clib_unload(CLibrary *cl);
void lj_clib_default(lua_State *L, GCtab *mt);

#endif

#endif
