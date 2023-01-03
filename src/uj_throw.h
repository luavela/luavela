/*
 * Implementation of throwing errors.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Only routines that implement complex logic (like invoking special handlers)
 * are provided as separate interfaces, simpler cases must be implemented
 * directly via uj_throw.
 */

#ifndef _UJ_THROW_H
#define _UJ_THROW_H

#include "lj_def.h"

struct lua_State;

/*
 * Language- and implementation-specific identifier of the kind of exception.
 * This is de facto only a common part of the identifiers produced by our
 * platform, "terminal" identifiers can be found in the implementation.
 * For the record, this constant is taken as is from the LuaJIT 2.0 sources.
 */
#define UJ_UEXCLASS 0x4c55414a49543200ULL /* LUAJIT2\0 */

/*
 * Throw an error designated with errcode by raising a DWARF2 exception.
 * Find catch frame, unwind stack and continue.
 */
LJ_NORET void uj_throw(struct lua_State *L, int errcode);

/*
 * Throw a runtime error. If the error function is defined, it is executed in
 * the context of L before unwinding the stack.
 */
LJ_NORET void uj_throw_run(struct lua_State *L);

/*
 * Throw a timeout error. If the timeout function is defined, it is executed in
 * the context of L before unwinding the stack.
 */
LJ_NORET void uj_throw_timeout(struct lua_State *L);

#endif /* !_UJ_THROW_H */
