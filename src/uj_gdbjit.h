/*
 * Client for the GDB JIT API.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _UJ_GDBJIT_H
#define _UJ_GDBJIT_H

#include "lj_obj.h"
#include "jit/lj_jit.h"

#if LJ_HASJIT && defined(GDBJIT)

void uj_gdbjit_addtrace(const jit_State *J, GCtrace *T);
void uj_gdbjit_deltrace(const jit_State *J, GCtrace *T);

#else /* LJ_HASJIT && defined(GDBJIT) */
#define uj_gdbjit_addtrace(J, T) UNUSED(T)
#define uj_gdbjit_deltrace(J, T) UNUSED(T)
#endif /* LJ_HASJIT && defined(GDBJIT) */

#endif /* !_UJ_GDBJIT_H */
