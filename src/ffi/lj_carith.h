/*
 * C data arithmetic.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _LJ_CARITH_H
#define _LJ_CARITH_H

#include "lj_obj.h"

#if LJ_HASFFI

int lj_carith_op(lua_State *L, enum MMS mm);
int lj_carith_len(lua_State *L);

uint64_t lj_carith_divu64(uint64_t a, uint64_t b);
int64_t lj_carith_divi64(int64_t a, int64_t b);
uint64_t lj_carith_modu64(uint64_t a, uint64_t b);
int64_t lj_carith_modi64(int64_t a, int64_t b);
uint64_t lj_carith_powu64(uint64_t x, uint64_t k);
int64_t lj_carith_powi64(int64_t x, int64_t k);

#endif

#endif
