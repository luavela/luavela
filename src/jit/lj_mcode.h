/*
 * Machine code management.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _LJ_MCODE_H
#define _LJ_MCODE_H

#if LJ_HASJIT || LJ_HASFFI
void lj_mcode_sync(void *start, void *end);
#endif

#if LJ_HASJIT

#include "jit/lj_jit.h"

void lj_mcode_free(jit_State *J);
MCode *lj_mcode_reserve(jit_State *J, MCode **lim);
void lj_mcode_commit(jit_State *J, MCode *m);
void lj_mcode_abort(jit_State *J);
LJ_NORET void lj_mcode_limiterr(jit_State *J, size_t need);

/* Unlocks the mcode area containing ptr.
** Ptr must be contained in one of mcode areas (checked internally), but
**  no alignment requirements are implied.
** This should be called prior to any changes in mcode of existing trace.
** Typically, enables write and disables execution permission until
**  lj_mcode_patch_finish is called.
** Returns pointer to the beginning of target mcode area. 
*/
MCode *lj_mcode_patch_start(jit_State *J, MCode *ptr);

/* Locks the mcode area pointed by ptr.
** Must be called after trace patch since execution permissions
**  are re-enabled here.
*/
void lj_mcode_patch_finish(jit_State *J, MCode *ptr);

#define lj_mcode_commitbot(J, m)        ((J)->mcbot = (m))

#endif

#endif
