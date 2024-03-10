/*
 * Interfaces for emitting global_State-related machine code.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _UJIT_JIT_EMIT_GL_H_
#define _UJIT_JIT_EMIT_GL_H_

void emit_rma(ASMState *, x86Op, Reg, const void *);

#define emit_opgl(as, xo, r, field) \
  emit_rma((as), (xo), (r), (void *)&J2G((as)->J)->field)
#define emit_getgl(as, r, field)        emit_opgl(as, XO_MOV, (r), field)
#define emit_setgl(as, r, field)        emit_opgl(as, XO_MOVto, (r), field)

#define emit_setvmstate(as, i) \
  (emit_i32(as, i), emit_opgl(as, XO_MOVmi, 0, vmstate))

#endif /* !_UJIT_JIT_EMIT_GL_H_ */
