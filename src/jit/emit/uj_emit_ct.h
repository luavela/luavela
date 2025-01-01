/*
 * Control transer instructions emitter.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Following instructions are supported (more to be added on demand):
 *
 *  * Short (rel8) conditional jumps.
 *  * Near (rel32) conditional jumps. Due to x64 encoding, conditional jumps
 *    that do not fit in rel32 are to be implemented as Jcc to jump pad
 *    containing JMP r/m64 (which is not supported).
 *  * Near (rel32) unconditional jumps. Unconditional jumps that fit in
 *    rel8 and rel16 are also encoded in 4 bytes. Jumps that do not fit in
 *    rel32 are not supported.
 *  * Near (rel32) procedure calls.
 *  * Near indirect procedure calls. For the targets that do not fit in rel32,
 *    target is loaded in RAX and `CALL r/m64` is emitted.
 */
#ifndef _UJ_EMIT_CT_H
#define _UJ_EMIT_CT_H

#include "jit/lj_jit.h"

/*
 * Mask used to retrieve actual condition code. Needed because:
 *   1. The `cc` variable used in the assembler carries not only condition code,
 *      but also some other pieces of data.
 *   2. Sometimes we want to look up condition code in the already emitted
 *      machine code, so we need to "distill" its value from the raw opcode.
 */
#define CONDITION_CODE_MASK 0xf

/*
 * Mask used to retrieve the 1-byte part of `jcc` opcodes
 * which is invariant to concrete condition codes.
 */
#define JCCS_OP_INVARIANT_MASK ((uint8_t)(~CONDITION_CODE_MASK))

/* Emit rel32 offset to target. */
int32_t uj_emit_rel32(const MCode *next_pc, const MCode *target);

/* Jcc rel8 */
void uj_emit_jccs(ASMState *as, x86CC cc, const MCode *target);

/* Jcc rel32 */
void uj_emit_jccn(ASMState *as, x86CC cc, const MCode *target);

/* JMP rel32 */
void uj_emit_jmp(ASMState *as, const MCode *target);

/* CALL rel32 or CALL r/m64 */
void uj_emit_call(ASMState *as, const void *function);

/*
 * Interfaces for emitting control flow back edges. In this case the target
 * is not known by the moment of Jcc emission. Thus, jcc should be first emitted
 * with uj_emit_jccs_backedge and fixed later by uj_emit_jcc_fixup.
 */

/* Return label pointing to current PC. */
const MCode *uj_emit_label(const ASMState *as);

/*
 * Jcc rel8, where rel8 is filled with a special value.
 * Returns pointer to the emitted instruction.
 */
MCode *uj_emit_jccs_backedge(ASMState *as, x86CC cc);

/* Patch `branch` by setting its jump target PC to `target`. */
void uj_emit_jcc_fixup(MCode *branch, const MCode *target);

#endif /* !_UJ_EMIT_CT_H */
