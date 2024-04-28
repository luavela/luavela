/*
 * IR assembler (SSA IR -> machine code).
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _LJ_ASM_H
#define _LJ_ASM_H

#include "jit/lj_jit.h"

#if LJ_HASJIT
void lj_asm_trace(jit_State *J, GCtrace *T);
void lj_asm_patchexit(jit_State *J, GCtrace *T, ExitNo exitno, MCode *target);
#endif

#endif
