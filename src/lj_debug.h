/*
 * Debugging and introspection.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _LJ_DEBUG_H
#define _LJ_DEBUG_H

#include "lj_obj.h"

typedef struct lj_Debug {
  /* Common fields. Must be in the same order as in lua.h. */
  int event;
  const char *name;
  const char *namewhat;
  const char *what;
  const char *source;
  int currentline;
  int nups;
  int linedefined;
  int lastlinedefined;
  char short_src[LUA_IDSIZE];
  int i_ci;
  /* Extended fields. Only valid if lj_debug_getinfo() is called with ext = 1.*/
  int nparams;
  int isvararg;
} lj_Debug;

const TValue *lj_debug_frame(lua_State *L, int level, int *size);
BCPos lj_debug_framepc(lua_State *L, GCfunc *fn, const TValue *nextframe);
BCLine lj_debug_frameline(lua_State *L, GCfunc *fn, const TValue *nextframe);
const char *lj_debug_slotname(GCproto *pt, const BCIns *pc,
                              BCReg slot, const char **name);
const char *lj_debug_funcname(lua_State *L, TValue *frame, const char **name);
void lj_debug_addloc(lua_State *L, const char *msg,
                     const TValue *frame, const TValue *nextframe);
int lj_debug_getinfo(lua_State *L, const char *what, lj_Debug *ar, int ext);

#endif
