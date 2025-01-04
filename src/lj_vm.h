/*
 * Assembler VM interface definitions.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _LJ_VM_H
#define _LJ_VM_H

#include "lj_obj.h"

/* Entry points for ASM parts of VM. */
void lj_vm_call(lua_State *L, TValue *base, int nres1);
int lj_vm_pcall(lua_State *L, TValue *base, int nres1, ptrdiff_t ef);
typedef TValue *(*lua_CPFunction)(lua_State *L, lua_CFunction func, void *ud);
int lj_vm_cpcall(lua_State *L, lua_CFunction func, void *ud,
                         lua_CPFunction cp);
int lj_vm_resume(lua_State *L, TValue *base, int nres1, ptrdiff_t ef);
LJ_NORET void lj_vm_unwind_c(void *cframe, int errcode);
LJ_NORET void lj_vm_unwind_ff(void *cframe);
void lj_vm_unwind_c_eh(void);
void lj_vm_unwind_ff_eh(void);
void lj_vm_unwind_rethrow(void);

/* Dispatch targets for recording and hooks. */
void lj_vm_record(void);
void lj_vm_inshook(void);
void lj_vm_rethook(void);
void lj_vm_callhook(void);

/* Trace exit handling. */
void lj_vm_exit_handler(void);
void lj_vm_exit_interp(void);

/* Continuations for metamethods. */
void lj_cont_cat(void);  /* Continue with concatenation. */
void lj_cont_ra(void);  /* Store result in RA from instruction. */
void lj_cont_nop(void);  /* Do nothing, just continue execution. */
void lj_cont_condt(void);  /* Branch if result is true. */
void lj_cont_condf(void);  /* Branch if result is false. */
void lj_cont_hook(void);  /* Continue from hook yield. */

enum { LJ_CONT_TAILCALL, LJ_CONT_FFI_CALLBACK };  /* Special continuations. */

/* Start of the ASM code. */
LJ_ASMENTRY char lj_vm_asm_begin[];

/* Internal assembler functions. Never call these directly from C. */
typedef void (*ASMFunction)(void);

#endif
