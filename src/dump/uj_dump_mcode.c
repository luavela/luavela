/*
 * C-level machine code dumper for assembled traces.
 * Port of LuaJIT's original machine code dumper dump.lua with some
 * modifications and reasonable simplifications (marked with NB!
 * throughout the code).
 * Disassembling itself is performed using udis86 library.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "udis86.h"
#include "jit/lj_ircall.h"
#include "dump/uj_dump_datadef.h"

#define MAX_SIDE_EXISTS (EXITSTUBS_PER_GROUP * LJ_MAX_EXITSTUBGR)

/* Check if an instruction is a jump. */
static LJ_AINLINE int dump_is_jump_ins(enum ud_mnemonic_code mnemo)
{
	return mnemo == UD_Ijmp || mnemo == UD_Ijz || mnemo == UD_Ijnz ||
	       mnemo == UD_Ijo || mnemo == UD_Ijno || mnemo == UD_Ijs ||
	       mnemo == UD_Ijns || mnemo == UD_Ijp || mnemo == UD_Ijnp ||
	       mnemo == UD_Ija || mnemo == UD_Ijbe || mnemo == UD_Ijb ||
	       mnemo == UD_Ijae || mnemo == UD_Ijg || mnemo == UD_Ijle ||
	       mnemo == UD_Ijl || mnemo == UD_Ijge || mnemo == UD_Ijcxz ||
	       mnemo == UD_Ijecxz || mnemo == UD_Ijrcxz;
}

/* Convert relative target displacement to the machine code address. */
static uintptr_t dump_abs_target(const ud_operand_t *op, uintptr_t pc)
{
	switch (op->size) {
	case 8: {
		return pc + op->lval.sbyte;
	}
	case 16: {
		return pc + op->lval.sword;
	}
	case 32: {
		return pc + op->lval.sdword;
	}
	}
	lua_assert(0); /* NB! INTERNAL: invalid relative offset size */
	return 0;
}

/*
 * NB! udis86 provides an ability to resolve symbols on the fly.
 * E.g. the code above transforms
 *    0bd5ffc1  call 0x4a49e4
 * to something like
 *    0bd5ffc1  call symbol+4294967295
 * This is not compatible with the original dump.lua output, but we can
 * implement it if we need it:)
 */
/*
static const char* ujit_mcode_sym_resolver(ud_t *disass, uint64_t addr,
					   int64_t *offset)
{
  UNUSED(disass);
  UNUSED(addr);

  *offset = 0xffffffffull;
  return "symbol";
}
*/

/*
 * Dump a LOOP target hint, stack check target hint
 * or side exit number hint, if either of the above is available.
 */
static void dump_try_resolve_exit(FILE *out, uintptr_t target,
				  uintptr_t loop_pc,
				  const uintptr_t *side_exits, uint16_t nexits)
{
	uint16_t i;

	if (target == loop_pc) {
		fprintf(out, "\t->LOOP");
		return;
	}

	for (i = 0; i < nexits; i++)
		if (target == side_exits[i]) {
			fprintf(out, "\t->%d", i);
			return;
		}
}

/* Dump ircall function name, if available. */
static void dump_try_resolve_call_target(FILE *out, const ud_operand_t *op,
					 uintptr_t pc)
{
	uint32_t i;
	uintptr_t target;

	/*
	 * NB! We assume that IR call targets can be encoded only with an
	 * immediate operand. So nothing to do here if this is not true.
	 */
	if (op->type != UD_OP_JIMM)
		return;

	target = dump_abs_target(op, pc);

	for (i = 0; i < IRCALL__MAX; i++)
		if (target == (uintptr_t)(lj_ir_callinfo[i].func)) {
			fprintf(out, "\t->%s", dump_ir_call_names[i]);
			return;
		}
}

void uj_dump_mcode(FILE *out, const jit_State *J, const GCtrace *trace)
{
	MCode *mcode_buffer;
	ud_t _disass;
	ud_t *disass;
	uintptr_t loop_pc;
	unsigned int bytes_parsed;
	uint16_t i;
	uint16_t nexits;
	uintptr_t side_exits[MAX_SIDE_EXISTS] = {0};

	if (trace->mcode == NULL)
		return;

	disass = &_disass;

	ud_init(disass);
	ud_set_mode(disass, 64);
	ud_set_syntax(disass, UD_SYN_INTEL);
	ud_set_vendor(disass, UD_VENDOR_ANY);

	/* ud_set_sym_resolver(disass, ujit_mcode_sym_resolver);*/

	/* Precalculate addresses of all possible exit stubs. */
	nexits = trace->nsnap;
	for (i = 0; i < nexits; i++)
		side_exits[i] = (uintptr_t)exitstub_addr(J, i);

	mcode_buffer = trace->mcode;
	loop_pc = (uintptr_t)mcode_buffer + (uintptr_t)trace->mcloop;

	ud_set_pc(disass, (uint64_t)mcode_buffer);
	ud_set_input_buffer(disass, (const unsigned char *)mcode_buffer,
			    trace->szmcode);

	fprintf(out, "---- TRACE %d mcode %llu\n", trace->traceno,
		(unsigned long long)(trace->szmcode));

	while ((bytes_parsed = ud_disassemble(disass))) {
		uintptr_t pc = (uintptr_t)ud_insn_off(disass);
		enum ud_mnemonic_code mnemo;
		const ud_operand_t *op1;

		if (pc == loop_pc && pc != (uintptr_t)mcode_buffer)
			fprintf(out, "-> LOOP:\n");

		fprintf(out, "%p  %-24s", (void *)pc, ud_insn_asm(disass));

		/*
		 * We have parsed current instruction and kinda point to the
		 * next one. Needed for correct dumping hints for
		 * jumps and calls.
		 */
		pc += bytes_parsed;

		mnemo = ud_insn_mnemonic(disass);
		op1 = ud_insn_opr(disass, 0);

		if (dump_is_jump_ins(mnemo)) {
			/*
			 * NB! We rely on the fact that side exit targets
			 * are always encoded as immediate operands.
			 */
			uintptr_t target;

			lua_assert(op1->type == UD_OP_JIMM);
			target = dump_abs_target(op1, pc);
			dump_try_resolve_exit(out, target, loop_pc, side_exits,
					      nexits);
		} else if (mnemo == UD_Icall) {
			dump_try_resolve_call_target(out, op1, pc);
		}

		fprintf(out, "\n");
	}
}
