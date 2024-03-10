/*
 * Control transer instructions emitter.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "jit/emit/lj_emit_x86.h"
#include "jit/emit/uj_emit_ct.h"

/* Sizes of relevant opcodes and corresponding instructions: */

#define SIZEOF_JCCS_OP sizeof(uint8_t)
#define SIZEOF_JCCS (SIZEOF_JCCS_OP + sizeof(uint8_t))

#define SIZEOF_JCCN_OP sizeof(uint16_t)
#define SIZEOF_JCCN (SIZEOF_JCCN_OP + sizeof(uint32_t))

#define SIZEOF_JMPN_OP sizeof(uint8_t)
#define SIZEOF_JMPN (SIZEOF_JMPN_OP + sizeof(uint32_t))

#define SIZEOF_CALL_REL32_OP sizeof(uint8_t)
#define SIZEOF_CALL_REL32 (SIZEOF_CALL_REL32_OP + sizeof(uint32_t))

#define JCCN_OP_PREFIX (0x0f) /* Common prefix for `jcc rel32` opcodes */

/*
 * Little-endian stub for 2-byte `JCC rel32` opcodes. In the Intel Software
 * Developer Manual there is a family of `0f 8x` instructions (where 'x'
 * denotes actual condition code).
 */
#define JCCN_OP_STUB (uint16_t)(0x800f)

/*
 * Little-endian mask used to retrieve the 2-byte part of near `jcc` opcodes
 * which is invariant to concrete condition codes.
 */
#define JCCN_OP_INVARIANT_MASK (uint16_t)(0xf0ff)

#define JCCS_BACKEDGE_DUMMY_TARGET ((int8_t)(-(SIZEOF_JCCS)))

/*
 * Control Transfer instruction. All currently supported instructions
 * (except for indirect procedure call) have following layout in memory:
 * +----+-----+
 * | op | rel |
 * +----+-----+
 */
struct ctins {
	MCode *pc; /* pc of the instruction being emitted */
	const MCode *next_pc; /* pc of the next instruction */
	void *op; /* opcode (variable-length) */
	void *rel; /* offset to target (variable-length, depends on op) */
};

/* Initialize and return instruction object. */
static struct ctins emit_ctins_init(const ASMState *as, size_t op_size,
				    size_t ins_size)
{
	MCode *pc = as->mcp - (ptrdiff_t)ins_size;
	struct ctins ins = {.pc = pc,
			    .next_pc = as->mcp,
			    .op = (void *)pc,
			    .rel = (void *)((uint8_t *)pc + op_size)};
	lua_assert(op_size < ins_size && op_size > 0 && ins_size > 0);
	return ins;
}

/* Finalize instruction emission. */
static LJ_AINLINE void emit_ctins_commit(ASMState *as, const struct ctins *ins)
{
	lua_assert(ins->pc < ins->next_pc);
	as->mcp = ins->pc;
}

/* Compute the opcode for short conditional jump. */
static LJ_AINLINE uint8_t emit_jccs_op(x86CC cc)
{
	return (uint8_t)(XI_JCCs + (cc & CONDITION_CODE_MASK));
}

/* Compute the opcode for near conditional jump, encode it as little-endian. */
static LJ_AINLINE uint16_t emit_jccn_op(x86CC cc)
{
	return (uint16_t)(JCCN_OP_STUB + ((cc & CONDITION_CODE_MASK) << 8));
}

#ifndef NDEBUG
/*
 * Compute relative 8 bit offset for short jump instructions.
 * Check that the offset fits in rel8.
 */
static LJ_AINLINE int emit_fits_rel8(ptrdiff_t delta)
{
	return delta == (int8_t)delta;
}
#endif /* !NDEBUG */

/*
 * Compute relative 32 bit offset for near jump and near call instructions.
 * Check that the offset fits in rel32.
 */
static LJ_AINLINE int emit_fits_rel32(ptrdiff_t delta)
{
	return delta == (int32_t)delta;
}

/* Emit rel8 offset to target. */
static LJ_AINLINE int8_t emit_rel8(const MCode *next_pc, const MCode *target)
{
	ptrdiff_t delta = target - next_pc;
	lua_assert(emit_fits_rel8(delta));
	return (int8_t)delta;
}

int32_t uj_emit_rel32(const MCode *next_pc, const MCode *target)
{
	ptrdiff_t delta = target - next_pc;
	lua_assert(emit_fits_rel32(delta));
	return (int32_t)delta;
}

/* Helper for emitting short conditional jumps. */
static MCode *emit_jccs(ASMState *as, x86CC cc, const MCode *target)
{
	struct ctins ins = emit_ctins_init(as, SIZEOF_JCCS_OP, SIZEOF_JCCS);
	*(uint8_t *)(ins.op) = emit_jccs_op(cc);
	*(int8_t *)(ins.rel) = NULL != target ? emit_rel8(ins.next_pc, target)
					      : JCCS_BACKEDGE_DUMMY_TARGET;
	emit_ctins_commit(as, &ins);
	return ins.pc;
}

static void emit_assert_jccs_fixup(const MCode *branch, ptrdiff_t delta)
{
#ifndef NDEBUG
	const uint8_t *op = (const uint8_t *)branch;
	const int8_t *rel8 = (const int8_t *)(op + SIZEOF_JCCS_OP);
	lua_assert((*op & JCCS_OP_INVARIANT_MASK) == XI_JCCs);
	lua_assert(emit_fits_rel8(delta));
	/*
	 * Assertions for the special value of rel8 and negative delta are here
	 * because currently we fix up only those `jcc rel8` instructions
	 * that were emitted with the uj_emit_jccs_backedge. Feel free to remove
	 * the assertions once this fix-up has to be generalized to other cases.
	 */
	lua_assert(*rel8 == JCCS_BACKEDGE_DUMMY_TARGET);
	lua_assert(delta < 0);
#else
	UNUSED(branch);
	UNUSED(delta);
#endif /* !NDEBUG */
}

static void emit_assert_jccn_fixup(const MCode *branch, ptrdiff_t delta)
{
#ifndef NDEBUG
	const uint16_t *op = (const uint16_t *)branch;
	lua_assert((*op & JCCN_OP_INVARIANT_MASK) == JCCN_OP_STUB);
	lua_assert(emit_fits_rel32(delta));
#else
	UNUSED(branch);
	UNUSED(delta);
#endif /* !NDEBUG */
}

static LJ_AINLINE void emit_jccs_fixup(MCode *branch, const MCode *target)
{
	ptrdiff_t delta = target - branch - SIZEOF_JCCS;
	emit_assert_jccs_fixup(branch, delta);
	*(int8_t *)(branch + SIZEOF_JCCS_OP) = (int8_t)delta;
}

static LJ_AINLINE void emit_jccn_fixup(MCode *branch, const MCode *target)
{
	ptrdiff_t delta = target - branch - SIZEOF_JCCN;
	emit_assert_jccn_fixup(branch, delta);
	*(int32_t *)(branch + SIZEOF_JCCN_OP) = (int32_t)delta;
}

/* CALL: near relative (rel32) procedure call. */
static void emit_call_rel32(ASMState *as, const MCode *target)
{
	struct ctins ins =
		emit_ctins_init(as, SIZEOF_CALL_REL32_OP, SIZEOF_CALL_REL32);
	*(uint8_t *)(ins.op) = XI_CALL;
	*(int32_t *)(ins.rel) = uj_emit_rel32(ins.next_pc, target);
	emit_ctins_commit(as, &ins);
}

/*
 * CALL: near indirect procedure call.
 * Assumes RID_RET is never an argument to calls and always clobbered.
 */
static LJ_AINLINE void emit_call_indirect(ASMState *as, const MCode *target)
{
	emit_rr(as, XO_GROUP5, XOg_CALL, RID_RET);
	emit_loadu64(as, RID_RET, (uint64_t)target);
}

void uj_emit_jccs(ASMState *as, x86CC cc, const MCode *target)
{
	lua_assert(NULL != target);
	emit_jccs(as, cc, target);
}

void uj_emit_jccn(ASMState *as, x86CC cc, const MCode *target)
{
	struct ctins ins = emit_ctins_init(as, SIZEOF_JCCN_OP, SIZEOF_JCCN);
	*(uint16_t *)(ins.op) = emit_jccn_op(cc);
	*(int32_t *)(ins.rel) = uj_emit_rel32(ins.next_pc, target);
	emit_ctins_commit(as, &ins);
}

void uj_emit_jmp(ASMState *as, const MCode *target)
{
	struct ctins ins = emit_ctins_init(as, SIZEOF_JMPN_OP, SIZEOF_JMPN);
	*(uint8_t *)(ins.op) = XI_JMP;
	*(int32_t *)(ins.rel) = uj_emit_rel32(ins.next_pc, target);
	emit_ctins_commit(as, &ins);
}

void uj_emit_call(ASMState *as, const void *function)
{
	const MCode *target = (const MCode *)function;
	if (emit_fits_rel32(target - as->mcp))
		emit_call_rel32(as, target);
	else
		emit_call_indirect(as, target);
}

const MCode *uj_emit_label(const ASMState *as)
{
	return (const MCode *)as->mcp;
}

MCode *uj_emit_jccs_backedge(ASMState *as, x86CC cc)
{
	return emit_jccs(as, cc, NULL);
}

void uj_emit_jcc_fixup(MCode *branch, const MCode *target)
{
	if (*(uint8_t *)branch == JCCN_OP_PREFIX)
		emit_jccn_fixup(branch, target);
	else
		emit_jccs_fixup(branch, target);
}
