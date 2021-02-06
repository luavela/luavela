/*
 * Implementation of the SSE2 instructions emission.
 * Implementation note. This emitter relies on common constants and macros
 * only to some extent. In particular, XO_ macros are not used (opcodes are
 * hardcoded), and the actual emission does not use emit_op. This is done
 * deliberately in order to:
 *  * Perform emission in natural (not backwards) order;
 *  * Avoid dependencies on the assembler state for easier testing.
 * Yep, this may be slower than the original emitter, but it does not seem
 * to be an issue so far.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "lj_def.h"
#include "jit/emit/lj_emit_x86.h"
#include "jit/lj_target_x86.h"

/* REX prefix and REX bits: */

#define REX_FIXED ((uint8_t)0x40) /* Fixed bit pattern */
#define REX_W ((uint8_t)0x08) /* Unused, defined for completeness */
#define REX_R ((uint8_t)0x04)
#define REX_X ((uint8_t)0x02) /* Unused, defined for completeness */
#define REX_B ((uint8_t)0x01)

/* Mandatory SIMD prefixes and opcodes: */

#define SIMD_PREFIX_U ((uint8_t)0xf3)
#define OP_MOVDQ_RM ((uint16_t)0x6f0f) /* NB! Byte order */
#define OP_MOVDQ_MR ((uint16_t)0x7f0f) /* NB! Byte order */

/*
 * movdqu xmm, XMMWORD PTR [gpr + ofs]
 * movdqu XMMWORD PTR [gpr + ofs], xmm
 */
static size_t emit_movdqu(uint8_t *mc, uint16_t op, uint8_t xmm, uint8_t gpr,
			  int32_t ofs)
{
	uint8_t *_mc = mc;
	uint8_t rex;
	x86Mode mod;

	lua_assert(op == OP_MOVDQ_MR || op == OP_MOVDQ_RM);
	lua_assert(xmm >= RID_MIN_FPR && xmm < RID_MAX_FPR);
	lua_assert(gpr >= RID_MIN_GPR && gpr < RID_MAX_GPR);

	*mc++ = SIMD_PREFIX_U;

	rex = 0;
	if (((xmm & 0x0f) & 7) != (xmm & 0x0f))
		rex |= REX_R;

	if ((gpr & 7) != gpr)
		rex |= REX_B;

	if (rex)
		*mc++ = (REX_FIXED | rex);

	*(uint16_t *)mc = op;
	mc += sizeof(int16_t);

	mod = XM_OFS0;
	if (ofs != 0 || (gpr & 7) == RID_EBP)
		mod = checki8(ofs) ? XM_OFS8 : XM_OFS32;

	*mc++ = MODRM(mod, xmm, gpr);

	if ((gpr & 7) == RID_ESP)
		*mc++ = SIB(XM_SCALE1, gpr, gpr);

	if (mod == XM_OFS8) {
		*mc++ = (int8_t)ofs;
	} else if (mod == XM_OFS32) {
		*(int32_t *)mc = ofs;
		mc += sizeof(int32_t);
	}

	return (size_t)(mc - _mc);
}

size_t uj_emit_movxmmrm(uint8_t *mc, uint8_t xmm, uint8_t gpr)
{
	return emit_movdqu(mc, OP_MOVDQ_RM, xmm, gpr, 0);
}

size_t uj_emit_movrmxmm(uint8_t *mc, uint8_t gpr, uint8_t xmm)
{
	return emit_movdqu(mc, OP_MOVDQ_MR, xmm, gpr, 0);
}

size_t uj_emit_spload_xmm(uint8_t *mc, uint8_t xmm, int32_t ofs)
{
	return emit_movdqu(mc, OP_MOVDQ_RM, xmm, RID_ESP, ofs);
}

size_t uj_emit_spstore_xmm(uint8_t *mc, uint8_t xmm, int32_t ofs)
{
	return emit_movdqu(mc, OP_MOVDQ_MR, xmm, RID_ESP, ofs);
}
