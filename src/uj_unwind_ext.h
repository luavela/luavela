/*
 * Interface of system-provided unwind handler.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * We have to use our own definitions instead of the mandatory (!) unwind.h,
 * since various OS, distros and compilers mess up the header installation.
 */

#ifndef _UJ_UNWIND_EXT_H
#define _UJ_UNWIND_EXT_H

#include <stdint.h>

struct _Unwind_Exception {
	uint64_t exclass;
	void (*excleanup)(int, struct _Unwind_Exception *);
	uintptr_t p1;
	uintptr_t p2;
} __attribute__((__aligned__));

struct _Unwind_Context;

#define _URC_OK 0
#define _URC_FATAL_PHASE1_ERROR 3
#define _URC_HANDLER_FOUND 6
#define _URC_INSTALL_CONTEXT 7
#define _URC_CONTINUE_UNWIND 8
#define _URC_FAILURE 9

#define _UA_SEARCH_PHASE 1
#define _UA_CLEANUP_PHASE 2
#define _UA_HANDLER_FRAME 4
#define _UA_FORCE_UNWIND 8

extern uintptr_t _Unwind_GetCFA(struct _Unwind_Context *);
extern void _Unwind_SetGR(struct _Unwind_Context *, int, uintptr_t);
extern void _Unwind_SetIP(struct _Unwind_Context *, uintptr_t);
extern void _Unwind_DeleteException(struct _Unwind_Exception *);
extern int _Unwind_RaiseException(struct _Unwind_Exception *);

#endif /* !_UJ_UNWIND_EXT_H */
