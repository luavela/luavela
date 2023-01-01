/*
 * Trace recorder for C data operations.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _LJ_CRECORD_H
#define _LJ_CRECORD_H

#include "lj_obj.h"
#include "jit/lj_jit.h"
#include "jit/lj_ffrecord.h"

#if LJ_HASJIT && LJ_HASFFI
void recff_ffi_meta___index(jit_State *J, RecordFFData *rd);
void recff_ffi_meta___newindex(jit_State *J, RecordFFData *rd);
void recff_cdata_call(jit_State *J, RecordFFData *rd);

void recff_ffi_meta___eq(jit_State *J, RecordFFData *rd);
void recff_ffi_meta___len(jit_State *J, RecordFFData *rd);
void recff_ffi_meta___lt(jit_State *J, RecordFFData *rd);
void recff_ffi_meta___le(jit_State *J, RecordFFData *rd);
void recff_ffi_meta___concat(jit_State *J, RecordFFData *rd);
void recff_ffi_meta___call(jit_State *J, RecordFFData *rd);
void recff_ffi_meta___add(jit_State *J, RecordFFData *rd);
void recff_ffi_meta___sub(jit_State *J, RecordFFData *rd);
void recff_ffi_meta___mul(jit_State *J, RecordFFData *rd);
void recff_ffi_meta___div(jit_State *J, RecordFFData *rd);
void recff_ffi_meta___mod(jit_State *J, RecordFFData *rd);
void recff_ffi_meta___pow(jit_State *J, RecordFFData *rd);
void recff_ffi_meta___unm(jit_State *J, RecordFFData *rd);


void recff_ffi_clib___index(jit_State *J, RecordFFData *rd);
void recff_ffi_clib___newindex(jit_State *J, RecordFFData *rd);


void recff_ffi_new(jit_State *J, RecordFFData *rd);
void recff_ffi_cast(jit_State *J, RecordFFData *rd);
void recff_ffi_errno(jit_State *J, RecordFFData *rd);
void recff_ffi_string(jit_State *J, RecordFFData *rd);
void recff_ffi_copy(jit_State *J, RecordFFData *rd);
void recff_ffi_fill(jit_State *J, RecordFFData *rd);
void recff_ffi_typeof(jit_State *J, RecordFFData *rd);
void recff_ffi_istype(jit_State *J, RecordFFData *rd);
void recff_ffi_abi(jit_State *J, RecordFFData *rd);
void recff_ffi_sizeof(jit_State *J, RecordFFData *rd);
void recff_ffi_alignof(jit_State *J, RecordFFData *rd);
void recff_ffi_offsetof(jit_State *J, RecordFFData *rd);
void recff_ffi_gc(jit_State *J, RecordFFData *rd);
void lj_crecord_tonumber(jit_State *J, RecordFFData *rd);
int lj_crecord_errno(void);
#endif

#endif
