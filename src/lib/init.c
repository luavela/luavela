/*
 * Library initialization.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 *
 * Major parts taken verbatim from the Lua interpreter.
 * Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
 */

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "lextlib.h"

#include "uj_arch.h"

static const luaL_Reg lj_lib_load[] = {
  { "",                 luaopen_base },
  { LUA_LOADLIBNAME,    luaopen_package },
  { LUA_TABLIBNAME,     luaopen_table },
  { LUA_IOLIBNAME,      luaopen_io },
  { LUA_OSLIBNAME,      luaopen_os },
  { LUA_STRLIBNAME,     luaopen_string },
  { LUA_MATHLIBNAME,    luaopen_math },
  { LUA_DBLIBNAME,      luaopen_debug },
  { LUAE_BITLIBNAME,     luaopen_bit },
  { LUAE_JITLIBNAME,     luaopen_jit },
  { LUAE_UJITLIBNAME,    luaopen_ujit },
  { NULL,               NULL }
};

static const luaL_Reg lj_lib_preload[] = {
#if LJ_HASFFI
  { LUAE_FFILIBNAME,     luaopen_ffi },
#endif
  { NULL,               NULL }
};

LUALIB_API void luaL_openlibs(lua_State *L)
{
  const luaL_Reg *lib;
  for (lib = lj_lib_load; lib->func; lib++) {
    lua_pushcfunction(L, lib->func);
    lua_pushstring(L, lib->name);
    lua_call(L, 1, 0);
  }
  luaL_findtable(L, LUA_REGISTRYINDEX, "_PRELOAD",
                 sizeof(lj_lib_preload)/sizeof(lj_lib_preload[0])-1);
  for (lib = lj_lib_preload; lib->func; lib++) {
    lua_pushcfunction(L, lib->func);
    lua_setfield(L, -2, lib->name);
  }
  lua_pop(L, 1);
}

