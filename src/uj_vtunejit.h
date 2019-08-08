/*
 * Client for the Intel VTune JIT API.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJ_VTUNEJIT_H
#define _UJ_VTUNEJIT_H

#include "lj_def.h"
#include "jit/lj_jit.h"

#if LJ_HASJIT && defined(VTUNEJIT)

/*
 * Add debug info for a newly compiled trace T and notify the profiler.
 * To be called after the trace is successfully assembled and saved.
 */
void uj_vtunejit_addtrace(const jit_State *J, GCtrace *T);

/*
 * Notify the profiler about changed machine code of the trace T.
 * To be called after the trace is patched.
 */
void uj_vtunejit_updtrace(const jit_State *J, const GCtrace *T);

/* Delete debug info for the trace T. To be called when a trace is deleted. */
void uj_vtunejit_deltrace(const jit_State *J, const GCtrace *T);

#else /* !(LJ_HASJIT && defined(VTUNEJIT)) */
#define uj_vtunejit_addtrace(J, T) UNUSED(T)
#define uj_vtunejit_updtrace(J, T) UNUSED(T)
#define uj_vtunejit_deltrace(J, T) UNUSED(T)
#endif /* LJ_HASJIT && defined(VTUNEJIT) */

#endif /* !_UJ_VTUNEJIT_H */
