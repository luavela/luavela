/*
 * Fast function call recorder.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _LJ_FFRECORD_H
#define _LJ_FFRECORD_H

#include "lj_obj.h"
#include "jit/lj_jit.h"

#if LJ_HASJIT
/* Data used by handlers to record a fast function. */
typedef struct RecordFFData {
  TValue *argv;         /* Runtime argument values. */
  ptrdiff_t nres;       /* Number of returned results (defaults to 1). */
} RecordFFData;

int32_t lj_ffrecord_select_mode(jit_State *J, TRef tr, TValue *tv);
void lj_ffrecord_func(jit_State *J);
#endif

#endif
