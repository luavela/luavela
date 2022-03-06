/*
 * Machine-dependent instruction folding for x86/x64
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "jit/lj_jit.h"
#include "jit/emit/emit_iface.h"

#define op_jcc_invar(ins) ((uint8_t)((ins) & JCCS_OP_INVARIANT_MASK))
#define op_jcc_cc(ins)    ((uint8_t)((ins) & CONDITION_CODE_MASK))

/*
 * Attempt to eliminate `test r, r` from
 *    op   r, ...
 *    test r, r
 *    jcc  label
 * ...with possible fixup of the jcc's control code. Example:
 *    add  r1, r2  ; sets OF
 *    test r1, r1  ; ...so this (r1 == 0) check is redundant
 *    jz   label
 * Implementation notes:
 *  * mcp points to the last emitted instruction
 *  * flagmcp points to the last emitted `test r, r`
 *  * `test r, r` is either 2 or 3 bytes long (without/with REX)
 *  * `jcc` opcode is either 1  or 2 bytes long (short/near conditional jump)
 */
void uj_opt_x86_fold_test_rr(ASMState *as)
{
	MCode *jcc_ins; /* pointer to jcc instruction */
	uint8_t *jcc;   /* part of the opcode to extract cc from */
	x86CC cc;

	if (as->flagmcp != as->mcp)
		return;

	/* Read the following instruction, should be a conditional jump: */
	jcc_ins = as->mcp + (*as->mcp < XI_TESTb ? 3 : 2);
	jcc = (uint8_t *)jcc_ins;

	if (*jcc == 0x0f) { /* near jump, 2 bytes long */
		jcc++;
		lua_assert(op_jcc_invar(*jcc) == XI_JCCn);
	} else { /* short jump, 1 byte long */
		lua_assert(op_jcc_invar(*jcc) == XI_JCCs);
	}

	cc = (x86CC)op_jcc_cc(*jcc);
	/* Cannot transform LE/NLE to cc without use of OF. */
	if (cc == CC_LE || cc == CC_NLE)
		return;

	lua_assert(cc == CC_E || cc == CC_NE || cc == CC_L || cc == CC_NL);

	/*
	 * We ignore OF and thus make following adjustments to the jumps
	 * (see x86CC enum for details): jl -> js, jnl -> jns.
	 */
	if (cc == CC_L || cc == CC_NL) {
		*jcc -= (CC_L - CC_S);
		lua_assert((cc == CC_L  && op_jcc_cc(*jcc) == CC_S)
			|| (cc == CC_NL && op_jcc_cc(*jcc) == CC_NS));
	}

	as->flagmcp = NULL;
	as->mcp = jcc_ins;
}
