/*
 * Snapshot handling.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _LJ_SNAP_H
#define _LJ_SNAP_H

#include "jit/lj_jit.h"

#if LJ_HASJIT
void lj_snap_add(jit_State *J);
void lj_snap_purge(jit_State *J);
void lj_snap_shrink(jit_State *J);
IRIns *lj_snap_regspmap(GCtrace *T, SnapNo snapno, IRIns *ir);
void lj_snap_replay(jit_State *J, GCtrace *T);
BCIns *lj_snap_restore(jit_State *J, void *exptr);
void lj_snap_grow_buf_(jit_State *J, size_t need);
void lj_snap_grow_map_(jit_State *J, size_t need);

static LJ_AINLINE void lj_snap_grow_buf(jit_State *J, size_t need)
{
  if (LJ_UNLIKELY(need > J->sizesnap)) lj_snap_grow_buf_(J, need);
}

static LJ_AINLINE void lj_snap_grow_map(jit_State *J, size_t need)
{
  if (LJ_UNLIKELY(need > J->sizesnapmap)) lj_snap_grow_map_(J, need);
}

#endif

#endif
