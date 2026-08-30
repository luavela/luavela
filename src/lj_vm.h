/*
 * Assembler VM interface definitions.
 * Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _LJ_VM_H
#define _LJ_VM_H

#include "lj_obj.h"
#include "uj_vm.h"

/* Entry points for ASM parts of VM. */
#ifndef UJIT_CINTERP
void lj_vm_call(lua_State *L, TValue *base, int nres1);
int lj_vm_pcall(lua_State *L, TValue *base, int nres1, ptrdiff_t ef);
int lj_vm_cpcall(lua_State *L, lua_CFunction func, void *ud,
                         lua_CPFunction cp);
int lj_vm_resume(lua_State *L, TValue *base, int nres1, ptrdiff_t ef);
LJ_NORET void lj_vm_unwind_c(void *cframe, int errcode);
LJ_NORET void lj_vm_unwind_ff(void *cframe);
void lj_vm_unwind_c_eh(void);
void lj_vm_unwind_ff_eh(void);
void lj_vm_unwind_rethrow(void);

/* Dispatch targets for recording and hooks. */

/* Trace exit handling. */
void lj_vm_exit_handler(void);
void lj_vm_exit_interp(void);
#else
#define lj_vm_call uj_vm_call
#define lj_vm_pcall uj_vm_pcall
#define lj_vm_cpcall uj_vm_cpcall
#define lj_vm_resume(L, base, unused1, unused2) uj_vm_resume(L, base)
#endif

enum { LJ_CONT_TAILCALL, LJ_CONT_FFI_CALLBACK };  /* Special continuations. */

void lj_vm_record(void);
void lj_vm_inshook(void);
void lj_vm_rethook(void);
void lj_vm_callhook(void);

#ifndef UJIT_CINTERP
/* Continuations for metamethods. */
void lj_cont_cat(void);  /* Continue with concatenation. */
void lj_cont_ra(void);  /* Store result in RA from instruction. */
void lj_cont_nop(void);  /* Do nothing, just continue execution. */
void lj_cont_condt(void);  /* Branch if result is true. */
void lj_cont_condf(void);  /* Branch if result is false. */
void lj_cont_hook(void);  /* Continue from hook yield. */

/* Start of the ASM code. */
LJ_ASMENTRY char lj_vm_asm_begin[];

/* Internal assembler functions. Never call these directly from C. */
typedef void (*ASMFunction)(void);
#else
#define lj_cont_cat uj_vm_cont_cat
#define lj_cont_ra uj_vm_cont_ra
#define lj_cont_nop uj_vm_cont_nop
#define lj_cont_condt uj_vm_cont_condt
#define lj_cont_condf uj_vm_cont_condf

UJ_PEDANTIC_OFF /* function declaration without a prototype */
typedef void *(*ASMFunction)();
UJ_PEDANTIC_ON
#endif

#endif
