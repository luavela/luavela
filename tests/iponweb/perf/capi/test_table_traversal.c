/*
 * Benchmark for lua_next vs. luaE_iterate.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <stdint.h>
#include <assert.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "lextlib.h"

int traverse_iterate(lua_State *L)
{
	lua_Integer n = 0;
	uint64_t iter = LUAE_ITER_BEGIN;
	while ((iter = luaE_iterate(L, 1, iter)) != LUAE_ITER_END) {
		n++;
		lua_pop(L, 2);
	}
	lua_pushinteger(L, n);
	return 1;
}

int traverse_next(lua_State *L)
{
	lua_Integer n = 0;
	lua_pushnil(L);
	while (lua_next(L, 1) != 0) {
		n++;
		lua_pop(L, 1);
	}
	lua_pushinteger(L, n);
	return 1;
}

int main(int argc, char const *argv[])
{
	lua_State *L;
	int rc_load;
	int rc_pcall;

	if (argc < 2)
		return 1;

	L = lua_open();
	assert(NULL != L);

	luaL_openlibs(L);

	lua_register(L, "traverse_iterate", traverse_iterate);
	lua_register(L, "traverse_next", traverse_next);

	rc_load = luaL_loadfile(L, argv[1]);

	if (0 != rc_load)
		return 1;

	rc_pcall = lua_pcall(L, 0, LUA_MULTRET, 0);

	if (0 != rc_pcall)
		return 1;

	lua_close(L);
	return 0;
}
