/*
 * Interfaces for Lua symbols extraction.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJPP_DEMANGLE_LUA_H
#define _UJPP_DEMANGLE_LUA_H

#define DEMANGLE_LUA_FAILED ((void *)-1)

struct vector;

void ujpp_demangle_lua(struct vector *lfunc_cache);

#endif /* !_UJPP_DEMANGLE_LUA_H */
