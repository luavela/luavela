/*
 * Indexed lods / store recorder.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _LJ_RECORD_INDEXED_H
#define _LJ_RECORD_INDEXED_H

#include "lj_ir.h"

struct jit_State;
struct RecordIndex;

/* Record indexed load/store. */
TRef uj_record_indexed(struct jit_State *J, struct RecordIndex *ix);

#endif /* !_LJ_RECORD_INDEXED_H */
