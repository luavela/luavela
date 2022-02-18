/*
 * Bytecode instruction modes.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "lj_obj.h"
#include "lj_bc.h"
#include "lj_bcdef.h"

/* External declarations for assembler functions accumulated below.
** These functions are never called from C code. Their pointers are
** stored in the known location and VM itself performs indirect jumps.
*/
#define BCENUM(name, ma, mb, mc, mt) void* lj_BC_##name(void);
BCDEF(BCENUM)
#undef BCENUM

#define FFENUM(name) void* lj_ff_##name(void);
ASMDEF(FFENUM)
#undef FFENUM

/* Default executors for real bytecodes and FF assembler counterparts.
** This is a reference mapping, actual runtime mapping is maintained by
** lj_dispatch.c
*/
LJ_DATADEF const ASMFunction lj_bc_ptr[] = {
  /* Executors for 'real' bytecode instructions.  */
  #define BCENUM(name, ma, mb, mc, mt) (ASMFunction)lj_BC_##name,
  BCDEF(BCENUM)
  #undef BCENUM

  /* Executors for 'pseudo' bytecode instructions (fast functions).  */
  #define FFENUM(name) (ASMFunction)lj_ff_##name,
  ASMDEF(FFENUM)
  #undef FFENUM

  NULL
};

/* Modes for bytecode instructions.
*/
LJ_DATADEF const uint16_t lj_bc_mode[] = {
  /* Bytecode modes for 'real' bytecode instructions.
  */
  BCDEF(BCMODE)

  /* Bytecode modes for 'pseudo' bytecode instructions (fast functions).
  */
  #define FFENUM(name) BCMODE_FF,
  ASMDEF(FFENUM)
  #undef FFENUM

  0
};
