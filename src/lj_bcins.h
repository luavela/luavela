/*
 * Common types for handling bytecodes.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _LJ_BCINS_H
#define _LJ_BCINS_H

#include <stdint.h>

typedef uint32_t BCIns;  /* Bytecode instruction. */
typedef uint32_t BCPos;  /* Bytecode position. */
typedef uint32_t BCReg;  /* Bytecode register. */
typedef  int32_t BCLine; /* Bytecode line number. */

/* Invalid bytecode position. */
#define NO_BCPOS (~(BCPos)0)

#endif /* !_LJ_BCINS_H */
