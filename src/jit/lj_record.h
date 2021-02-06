/*
 * Trace recorder (bytecode -> SSA IR).
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _LJ_RECORD_H
#define _LJ_RECORD_H

#include "lj_obj.h"
#include "jit/lj_jit.h"
#include "lj_vm.h"

#if LJ_HASJIT
/* Context for recording an indexed load/store. */
typedef struct RecordIndex {
  TValue tabv;          /* Runtime value of table (or indexed object). */
  TValue keyv;          /* Runtime value of key. */
  TValue valv;          /* Runtime value of stored value. */
  TValue mobjv;         /* Runtime value of metamethod object. */
  GCtab *mtv;           /* Runtime value of metatable object. */
  const TValue *oldv;   /* Runtime value of previously stored value. */
  TRef tab;             /* Table (or indexed object) reference. */
  TRef key;             /* Key reference. */
  TRef val;             /* Value reference for a store or 0 for a load. */
  TRef mt;              /* Metatable reference. */
  TRef mobj;            /* Metamethod object reference. */
  int idxchain;         /* Index indirections left or 0 for raw lookup. */
  IROp loadop;          /* Load operation from indexed loads / stores. */
  IROp xrefop;          /* Lookup oprations type. */
  TRef xref;            /* Reference to AREF / HREF{K}. */
  const TValue *oldval; /* Previously stored value, based on oldv. */
} RecordIndex;

int lj_record_objcmp(jit_State *J, TRef a, TRef b,
                     const TValue *av, const TValue *bv);
TRef lj_record_constify(jit_State *J, const TValue *o);

void lj_record_call(jit_State *J, BCReg func, ptrdiff_t nargs);
void lj_record_tailcall(jit_State *J, BCReg func, ptrdiff_t nargs);
void lj_record_ret(jit_State *J, BCReg rbase, ptrdiff_t gotresults);

int lj_record_mm_lookup(jit_State *J, RecordIndex *ix, enum MMS mm);

void lj_record_ins(jit_State *J);
void lj_record_setup(jit_State *J);
BCReg lj_record_mm_prep(jit_State *J, ASMFunction cont);
void lj_record_idx_abc(jit_State *J, TRef asizeref, TRef ikey, uint32_t asize);

#endif

#endif
