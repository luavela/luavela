/*
 * Hook management interfaces.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _UJ_HOOK_H
#define _UJ_HOOK_H

#include "lj_vm.h"

#if LJ_HASFFI && !defined(_BUILDVM_H)
/* Save/restore errno and GetLastError() around hooks, exits and recording. */
#include <errno.h>

static LJ_AINLINE int errno_save(void)
{
	return errno;
}

static LJ_AINLINE void errno_restore(int old_errno)
{
	errno = old_errno;
}
#else /* LJ_HASFFI && !defined(_BUILDVM_H) */
static LJ_AINLINE int errno_save(void)
{
	return 0;
}

static LJ_AINLINE void errno_restore(int old_errno)
{
	UNUSED(old_errno);
}
#endif /* LJ_HASFFI && !defined(_BUILDVM_H) */

/* Instruction dispatch callback for hooks or when recording. */
void uj_hook_ins(struct lua_State *L, const BCIns *pc);
ASMFunction uj_hook_call(struct lua_State *L, const BCIns *pc);

#endif /* !_UJ_HOOK_H */
