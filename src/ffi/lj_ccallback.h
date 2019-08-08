/*
 * FFI C callback handling.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _LJ_CCALLBACK_H
#define _LJ_CCALLBACK_H

#include "lj_obj.h"
#include "ffi/lj_ctype.h"

#if LJ_HASFFI

/* Really belongs to lj_vm.h. */
void lj_vm_ffi_callback(void);

size_t lj_ccallback_ptr2slot(CTState *cts, void *p);
lua_State * lj_ccallback_enter(CTState *cts, void *cf);
void lj_ccallback_leave(CTState *cts, TValue *o);
void *lj_ccallback_new(CTState *cts, CType *ct, GCfunc *fn);
void lj_ccallback_mcode_free(CTState *cts);

#endif

#endif
