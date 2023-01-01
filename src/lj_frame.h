/*
 * Guest stack frames.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _LJ_FRAME_H
#define _LJ_FRAME_H

#include "lj_obj.h"
#include "lj_bc.h"
#include "lj_vm.h"

/* -- Lua stack frame ----------------------------------------------------- */

/* Frame type markers in callee function slot (callee base-1). */

/* FRAME_PCALLH is using in hooks, FRAME_LUAP seems not to be used
** because there is conflict with FRAME_LUA is some cases.
*/
enum {
  FRAME_LUA, FRAME_C, FRAME_CONT, FRAME_VARG,
  FRAME_LUAP, FRAME_CP, FRAME_PCALL, FRAME_PCALLH
};
#define FRAME_TYPE              3
#define FRAME_P                 4
#define FRAME_TYPEP             (FRAME_TYPE|FRAME_P)

/* Macros to access and modify Lua frames. */
#define frame_gc(f)             ((f)->fr.func)
#define frame_func(f)           (gco2func(frame_gc(f)))

#define frame_type(f)           (frame_ftsz(f) & FRAME_TYPE)
#define frame_typep(f)          (frame_ftsz(f) & FRAME_TYPEP)
#define frame_islua(f)          (frame_type(f) == FRAME_LUA && frame_ftsz(f))
#define frame_isc(f)            (frame_type(f) == FRAME_C)
#define frame_iscont(f)         (frame_typep(f) == FRAME_CONT)
#define frame_isvarg(f)         (frame_typep(f) == FRAME_VARG)
#define frame_ispcall(f)        ((frame_ftsz(f) & FRAME_PCALL) == FRAME_PCALL)

#define frame_contpc(f)         (frame_pc((f)-1))

LJ_AINLINE ASMFunction frame_contf(const TValue *f)
{
UJ_PEDANTIC_OFF /* casting void* to a function ptr */
  return ((ASMFunction)(void *)((intptr_t)lj_vm_asm_begin +
                         (intptr_t)(int32_t)(f-1)->u32.lo));
UJ_PEDANTIC_ON
}

LJ_AINLINE void setcont(TValue *o, ASMFunction f)
{
UJ_PEDANTIC_OFF /* casting a function ptr to void* */
  o->u64 = (uint64_t)(void *)f - (uint64_t)lj_vm_asm_begin;
UJ_PEDANTIC_ON
}

#define frame_sized(f)          (frame_ftsz(f) & ~FRAME_TYPEP)
#define frame_delta(f)          (frame_sized(f) / sizeof(TValue))
#define frame_deltal(f)         (1 + bc_a(frame_pc(f)[-1]))

#define frame_prevl(f)          ((f) - frame_deltal(f))
#define frame_prevd(f)          ((TValue *)((char *)(f) - frame_sized(f)))
#define frame_prev(f)           (frame_islua(f)?frame_prevl(f):frame_prevd(f))
/* Note: this macro does not skip over FRAME_VARG. */

#define setframe_gc(f, p)       ((f)->fr.func = (p))

#define frame_ftsz(f)           ((f)->fr.tp.ftsz)
#define setframe_ftsz(f, sz)    (frame_ftsz(f) = (sz))

#define frame_pc(f)             ((f)->fr.tp.pcr)
#define setframe_pc(f, pc)      (frame_pc(f) = (pc))

/* NB! When stack for a new coroutine is created, corresponding lua_State is
** stored at its bottom, so the empty stack right after initialization
** has following layout (growth upwards):
** [slot:       nil] <-top <-base
** [slot: lua_State] <-stack
** Situation when lua_State (i.e. a non-functional object)
** is located at (base - 1) is called a dummy frame.
*/
#define BOTTOM_FRAME_FTSZ       0

/* There are 2 special framelinks in uJIT: dummy and bottom frames.
** In some cases functions like meta_err_optype_call can put L on stack with
** valid ftsz. So we have opportunity for stack unwinding, but we need to
** know current L. This frame is called dummy frame.
*/
static LJ_AINLINE int frame_isdummy(const lua_State *L, const TValue *st) {
  return obj2gco(L) == frame_gc(st);
}

static LJ_AINLINE void frame_set_dummy(const lua_State *L, TValue *st, int64_t ftsz) {
  setframe_gc(st, obj2gco(L));
  setframe_ftsz(st, ftsz);
}

/* Bottom frame is what L->stack points to. It has ftsz set to 0 (because there
** is no caller and no stack unwinding is needed) and it's easy to detect this
** frame without knowledge of L. frame_isdummy() is able to detect bottom
** frame too.
*/
static LJ_AINLINE int frame_isbottom(const TValue *st) {
  return frame_ftsz(st) == BOTTOM_FRAME_FTSZ;
}

static LJ_AINLINE void frame_set_bottom(const lua_State *L, TValue *st) {
  frame_set_dummy(L, st, BOTTOM_FRAME_FTSZ);
}

#if LJ_HASFFI
static LJ_AINLINE int frame_is_cont_ffi_cb(const TValue *frame) {
  return LJ_CONT_FFI_CALLBACK == (frame - 1)->u32.lo;
}
#endif /* LJ_HASFFI */

/*
 * Frame type in case of stack unwinding during error handling. Contains a quirk
 * that allows to treat some continuation frames as "normal" C frames in case
 * FFI is enabled.
 */
static LJ_AINLINE uint8_t frame_type_unwind(const TValue *frame) {
  uint8_t type = (uint8_t)frame_typep(frame);

#if LJ_HASFFI
  if (FRAME_CONT == type && frame_is_cont_ffi_cb(frame))
    type = FRAME_C;
#endif /* LJ_HASFFI */

  lua_assert(type >= FRAME_LUA && type <= FRAME_PCALLH);
  return type;
}

#endif
