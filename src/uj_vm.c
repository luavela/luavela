/*
 * Bytecode interpreter implemented in C.
 * Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef NDEBUG
#define NDEBUG
#endif

#include "lj_frame.h"
#include "lj_gc.h"
#include "lj_tab.h"
#include "uj_func.h"
#include "uj_lib.h"
#include "uj_meta.h"
#include "uj_mtab.h"
#include "uj_state.h"
#include "uj_throw.h"
#include "uj_timerint.h"
#include "uj_upval.h"
#include "uj_vm.h"
#include "uj_sbuf.h"
#include "uj_coverage.h"
#include "uj_hook.h"
#include "uj_dispatch.h"

#define vm_raw_ra(ins) (((ins) >> 8) & 0xff)
#define vm_raw_rb(ins) ((ins) >> 24)
#define vm_raw_rc(ins) (((ins) >> 16) & 0xff)
#define vm_raw_rd(ins) (((ins) >> 16))
/* Jump targets are not x2-encoded, use the raw bc register value */
#define vm_rj(ins) ((ptrdiff_t)vm_raw_rd(ins) - BCBIAS_J)

#define vm_index_ra(ins) (vm_raw_ra(ins) >> 1)

// clang-format off
#define vm_slot_ra(base, ins) \
	(TValue *)((char *)(base) + (ptrdiff_t)vm_raw_ra(ins) * sizeof(TValue) / 2)
#define vm_slot_rb(base, ins) \
	(TValue *)((char *)(base) + (ptrdiff_t)vm_raw_rb(ins) * sizeof(TValue) / 2)
#define vm_slot_rc(base, ins) \
	(TValue *)((char *)(base) + (ptrdiff_t)vm_raw_rc(ins) * sizeof(TValue) / 2)
#define vm_slot_rd(base, ins) \
	(TValue *)((char *)(base) + (ptrdiff_t)vm_raw_rd(ins) * sizeof(TValue) / 2)
#define vm_kbase_gco(kbase, kindex) \
	((GCobj *)*((GCobj **)(kbase) + (int16_t)~(uint16_t)(kindex)))
// clang-format on

#define base2func(base) ((((base) - 1)->fr.func)->fn)
#define vm_base_upval(base, uvindex) (base2func(base).l.uvptr[uvindex])
#define pc2proto(pc) ((const GCproto *)(pc) - 1)

#define save_PC(pc) vmf->pc = (pc)
#define restore_PC(base) ((base) - 1)->fr.tp.pcr
#define restore_base(base, pc) \
	((base) - (int8_t)((*((uint8_t *)(pc) - 3) >> 1) + 1))
#define setup_kbase(base) pc2proto(base2func(base).l.pc)->k

#define save_L(L) vmf->L = (L)
/*
 * Get frame offset for non-standard caller frame.
 * Synonym for frame_sized(reg) (see lj_frame.h)
 */
#define ftsz2offs(pc) ((uint64_t)(uintptr_t)(pc) & (uint64_t)(-8))

#define mm_call_ftsz(top, base) \
	(int64_t)((uintptr_t)(top) - (uintptr_t)(base) + FRAME_CONT)

/* LuaJIT VM always has number of arguments or return parameters increased by 1 */
#define FFUNC_NARGS(N) ((N) + 1)

#define HANDLER_SIGNATURE \
	BCIns *pc, TValue *base, struct vm_frame *vmf, BCIns ins

#define HANDLER_ARGUMENTS pc, base, vmf, ins

// clang-format off
#define vm_next(pc, base, vmf) \
	(L2GG((vmf)->L)->dispatch_cinterp[bc_op(*(pc))])((pc) + 1, (base), (vmf), *(pc))
// clang-format on

#define DISPATCH() vm_next(pc, base, vmf)

/* These ones piggyback nargs1 to the prologue. Note that nargs1 is 16-bits wide */
#define vm_next_call(pc, base, vmf, nargs1)               \
	(L2GG((vmf)->L)->dispatch_cinterp[bc_op(*(pc))])( \
		(pc) + 1, (base), (vmf), *(pc) | ((nargs1) << 16))

#define DISPATCH_CALL(nargs1) vm_next_call(pc, base, vmf, nargs1)

LJ_STATIC_ASSERT(offsetof(struct vm_frame, c_interp_marker) ==
		 offsetof(struct cframe, savereg_r14));
LJ_STATIC_ASSERT(offsetof(struct vm_frame, saved_gpr) == 96);
#if __x86_64__
LJ_STATIC_ASSERT(sizeof(struct vm_frame) == 160);
#else
LJ_STATIC_ASSERT(sizeof(struct vm_frame) == 344);
LJ_STATIC_ASSERT(offsetof(struct vm_frame, saved_fpr) == 192);
LJ_STATIC_ASSERT(offsetof(struct vm_frame, entry_func_return_addr) == 328);
LJ_STATIC_ASSERT(offsetof(struct vm_frame, vm_return_addr) == 336);
#endif

static void *uj_BC_NYI(HANDLER_SIGNATURE);
static void *uj_BC_ISLT(HANDLER_SIGNATURE);
static void *uj_BC_ISGE(HANDLER_SIGNATURE);
static void *uj_BC_ISLE(HANDLER_SIGNATURE);
static void *uj_BC_ISGT(HANDLER_SIGNATURE);
static void *uj_BC_ISEQV(HANDLER_SIGNATURE);
static void *uj_BC_ISNEV(HANDLER_SIGNATURE);
static void *uj_BC_ISEQS(HANDLER_SIGNATURE);
static void *uj_BC_ISNES(HANDLER_SIGNATURE);
static void *uj_BC_ISEQN(HANDLER_SIGNATURE);
static void *uj_BC_ISNEN(HANDLER_SIGNATURE);
static void *uj_BC_ISEQP(HANDLER_SIGNATURE);
static void *uj_BC_ISNEP(HANDLER_SIGNATURE);
static void *uj_BC_ISTC(HANDLER_SIGNATURE);
static void *uj_BC_ISFC(HANDLER_SIGNATURE);
static void *uj_BC_IST(HANDLER_SIGNATURE);
static void *uj_BC_ISF(HANDLER_SIGNATURE);
static void *uj_BC_MOV(HANDLER_SIGNATURE);
static void *uj_BC_NOT(HANDLER_SIGNATURE);
static void *uj_BC_UNM(HANDLER_SIGNATURE);
static void *uj_BC_LEN(HANDLER_SIGNATURE);
static void *uj_BC_ADD(HANDLER_SIGNATURE);
static void *uj_BC_SUB(HANDLER_SIGNATURE);
static void *uj_BC_MUL(HANDLER_SIGNATURE);
static void *uj_BC_DIV(HANDLER_SIGNATURE);
static void *uj_BC_MOD(HANDLER_SIGNATURE);
static void *uj_BC_POW(HANDLER_SIGNATURE);
static void *uj_BC_CAT(HANDLER_SIGNATURE);
static void *uj_BC_KSTR(HANDLER_SIGNATURE);
static void *uj_BC_KCDATA(HANDLER_SIGNATURE);
static void *uj_BC_KSHORT(HANDLER_SIGNATURE);
static void *uj_BC_KNUM(HANDLER_SIGNATURE);
static void *uj_BC_KPRI(HANDLER_SIGNATURE);
static void *uj_BC_KNIL(HANDLER_SIGNATURE);
static void *uj_BC_UGET(HANDLER_SIGNATURE);
static void *uj_BC_USETV(HANDLER_SIGNATURE);
static void *uj_BC_USETS(HANDLER_SIGNATURE);
static void *uj_BC_USETN(HANDLER_SIGNATURE);
static void *uj_BC_USETP(HANDLER_SIGNATURE);
static void *uj_BC_UCLO(HANDLER_SIGNATURE);
static void *uj_BC_FNEW(HANDLER_SIGNATURE);
static void *uj_BC_TNEW(HANDLER_SIGNATURE);
static void *uj_BC_TDUP(HANDLER_SIGNATURE);
static void *uj_BC_GGET(HANDLER_SIGNATURE);
static void *uj_BC_GSET(HANDLER_SIGNATURE);
static void *uj_BC_TGETV(HANDLER_SIGNATURE);
static void *uj_BC_TGETS(HANDLER_SIGNATURE);
static void *uj_BC_TGETB(HANDLER_SIGNATURE);
static void *uj_BC_TSETV(HANDLER_SIGNATURE);
static void *uj_BC_TSETS(HANDLER_SIGNATURE);
static void *uj_BC_TSETB(HANDLER_SIGNATURE);
static void *uj_BC_TSETM(HANDLER_SIGNATURE);
static void *uj_BC_CALLM(HANDLER_SIGNATURE);
static void *uj_BC_CALL(HANDLER_SIGNATURE);
static void *uj_BC_CALLMT(HANDLER_SIGNATURE);
static void *uj_BC_CALLT(HANDLER_SIGNATURE);
static void *uj_BC_ITERC(HANDLER_SIGNATURE);
static void *uj_BC_ITERN(HANDLER_SIGNATURE);
static void *uj_BC_VARG(HANDLER_SIGNATURE);
static void *uj_BC_ISNEXT(HANDLER_SIGNATURE);
static void *uj_BC_RETM(HANDLER_SIGNATURE);
static void *uj_BC_RET(HANDLER_SIGNATURE);
static void *uj_BC_RET0(HANDLER_SIGNATURE);
static void *uj_BC_RET1(HANDLER_SIGNATURE);
static void *uj_BC_HOTCNT(HANDLER_SIGNATURE);
static void *uj_BC_COVERG(HANDLER_SIGNATURE);
static void *uj_BC_FORI(HANDLER_SIGNATURE);
static void *uj_BC_IFORL(HANDLER_SIGNATURE);
static void *uj_BC_IITERL(HANDLER_SIGNATURE);
static void *uj_BC_IITRNL(HANDLER_SIGNATURE);
static void *uj_BC_ILOOP(HANDLER_SIGNATURE);
static void *uj_BC_JMP(HANDLER_SIGNATURE);
static void *uj_BC_IFUNCF(HANDLER_SIGNATURE);
static void *uj_BC_IFUNCV(HANDLER_SIGNATURE);
static void *uj_BC_FUNCC(HANDLER_SIGNATURE);
static void *uj_BC_FUNCCW(HANDLER_SIGNATURE);

static void *uj_ff_assert(HANDLER_SIGNATURE);
static void *uj_ff_type(HANDLER_SIGNATURE);
static void *uj_ff_next(HANDLER_SIGNATURE);
static void *uj_ff_pairs(HANDLER_SIGNATURE);
static void *uj_ff_ipairs_aux(HANDLER_SIGNATURE);
static void *uj_ff_ipairs(HANDLER_SIGNATURE);
static void *uj_ff_getmetatable(HANDLER_SIGNATURE);
static void *uj_ff_setmetatable(HANDLER_SIGNATURE);
static void *uj_ff_rawget(HANDLER_SIGNATURE);
static void *uj_ff_tonumber(HANDLER_SIGNATURE);
static void *uj_ff_tostring(HANDLER_SIGNATURE);
static void *uj_ff_pcall(HANDLER_SIGNATURE);
static void *uj_ff_xpcall(HANDLER_SIGNATURE);
static void *uj_ff_coroutine_resume(HANDLER_SIGNATURE);
static void *uj_ff_coroutine_yield(HANDLER_SIGNATURE);
static void *uj_ff_coroutine_wrap_aux(HANDLER_SIGNATURE);
static void *uj_ff_math_fabs(HANDLER_SIGNATURE);
static void *uj_ff_math_floor(HANDLER_SIGNATURE);
static void *uj_ff_math_ceil(HANDLER_SIGNATURE);
static void *uj_ff_math_sqrt(HANDLER_SIGNATURE);
static void *uj_ff_math_log10(HANDLER_SIGNATURE);
static void *uj_ff_math_exp(HANDLER_SIGNATURE);
static void *uj_ff_math_sin(HANDLER_SIGNATURE);
static void *uj_ff_math_cos(HANDLER_SIGNATURE);
static void *uj_ff_math_tan(HANDLER_SIGNATURE);
static void *uj_ff_math_asin(HANDLER_SIGNATURE);
static void *uj_ff_math_acos(HANDLER_SIGNATURE);
static void *uj_ff_math_atan(HANDLER_SIGNATURE);
static void *uj_ff_math_sinh(HANDLER_SIGNATURE);
static void *uj_ff_math_cosh(HANDLER_SIGNATURE);
static void *uj_ff_math_tanh(HANDLER_SIGNATURE);
static void *uj_ff_math_frexp(HANDLER_SIGNATURE);
static void *uj_ff_math_modf(HANDLER_SIGNATURE);
static void *uj_ff_math_rad_deg(HANDLER_SIGNATURE);
static void *uj_ff_math_log(HANDLER_SIGNATURE);
static void *uj_ff_math_atan2(HANDLER_SIGNATURE);
static void *uj_ff_math_pow(HANDLER_SIGNATURE);
static void *uj_ff_math_fmod(HANDLER_SIGNATURE);
static void *uj_ff_math_ldexp(HANDLER_SIGNATURE);
static void *uj_ff_math_min(HANDLER_SIGNATURE);
static void *uj_ff_math_max(HANDLER_SIGNATURE);
static void *uj_ff_bit_tobit(HANDLER_SIGNATURE);
static void *uj_ff_bit_bnot(HANDLER_SIGNATURE);
static void *uj_ff_bit_bswap(HANDLER_SIGNATURE);
static void *uj_ff_bit_lshift(HANDLER_SIGNATURE);
static void *uj_ff_bit_rshift(HANDLER_SIGNATURE);
static void *uj_ff_bit_arshift(HANDLER_SIGNATURE);
static void *uj_ff_bit_rol(HANDLER_SIGNATURE);
static void *uj_ff_bit_ror(HANDLER_SIGNATURE);
static void *uj_ff_bit_band(HANDLER_SIGNATURE);
static void *uj_ff_bit_bor(HANDLER_SIGNATURE);
static void *uj_ff_bit_bxor(HANDLER_SIGNATURE);
static void *uj_ff_string_len(HANDLER_SIGNATURE);
static void *uj_ff_string_byte(HANDLER_SIGNATURE);
static void *uj_ff_string_char(HANDLER_SIGNATURE);
static void *uj_ff_string_sub(HANDLER_SIGNATURE);
static void *uj_ff_string_rep(HANDLER_SIGNATURE);
static void *uj_ff_string_reverse(HANDLER_SIGNATURE);
static void *uj_ff_string_lower(HANDLER_SIGNATURE);
static void *uj_ff_string_upper(HANDLER_SIGNATURE);
static void *uj_ff_table_getn(HANDLER_SIGNATURE);

static void *vm_return(HANDLER_SIGNATURE, ptrdiff_t ra_offset);
static void *vm_returnc(HANDLER_SIGNATURE, ptrdiff_t ra_offset, int nres);
static uint16_t vm_vmeta_call(HANDLER_SIGNATURE, TValue *fn, uint16_t nargs1);
static void *vm_call_dispatch(HANDLER_SIGNATURE, TValue *oldbase,
			      uint16_t nargs1);
static void *vm_bc_cat_z(HANDLER_SIGNATURE, TValue *src_start, TValue *src_end);
/* Defined in src/lib/base.c */
void lj_ffh_coroutine_wrap_err(lua_State *L, lua_State *co);

const CInterpFunction uj_vm_bc_dispatch_cinterp[] = {
	uj_BC_ISLT, /* 0x00 ISLT */
	uj_BC_ISGE, /* 0x01 ISGE */
	uj_BC_ISLE, /* 0x02 ISLE */
	uj_BC_ISGT, /* 0x03 ISGT */
	uj_BC_ISEQV, /* 0x04 ISEQV */
	uj_BC_ISNEV, /* 0x05 ISNEV */
	uj_BC_ISEQS, /* 0x06 ISEQS */
	uj_BC_ISNES, /* 0x07 ISNES */
	uj_BC_ISEQN, /* 0x08 ISEQN */
	uj_BC_ISNEN, /* 0x09 ISNEN */
	uj_BC_ISEQP, /* 0x0a ISEQP */
	uj_BC_ISNEP, /* 0x0b ISNEP */
	uj_BC_ISTC, /* 0x0c ISTC */
	uj_BC_ISFC, /* 0x0d ISFC */
	uj_BC_IST, /* 0x0e IST */
	uj_BC_ISF, /* 0x0f ISF */
	uj_BC_MOV, /* 0x10 MOV */
	uj_BC_NOT, /* 0x11 NOT */
	uj_BC_UNM, /* 0x12 UNM */
	uj_BC_LEN, /* 0x13 LEN */
	uj_BC_ADD, /* 0x14 ADD */
	uj_BC_SUB, /* 0x15 SUB */
	uj_BC_MUL, /* 0x16 MUL */
	uj_BC_DIV, /* 0x17 DIV */
	uj_BC_MOD, /* 0x18 MOD */
	uj_BC_POW, /* 0x19 POW */
	uj_BC_CAT, /* 0x1a CAT */
	uj_BC_KSTR, /* 0x1b KSTR */
	uj_BC_KCDATA, /* 0x1c KCDATA */
	uj_BC_KSHORT, /* 0x1d KSHORT */
	uj_BC_KNUM, /* 0x1e KNUM */
	uj_BC_KPRI, /* 0x1f KPRI */
	uj_BC_KNIL, /* 0x20 KNIL */
	uj_BC_UGET, /* 0x21 UGET */
	uj_BC_USETV, /* 0x22 USETV */
	uj_BC_USETS, /* 0x23 USETS */
	uj_BC_USETN, /* 0x24 USETN */
	uj_BC_USETP, /* 0x25 USETP */
	uj_BC_UCLO, /* 0x26 UCLO */
	uj_BC_FNEW, /* 0x27 FNEW */
	uj_BC_TNEW, /* 0x28 TNEW */
	uj_BC_TDUP, /* 0x29 TDUP */
	uj_BC_GGET, /* 0x2a GGET */
	uj_BC_GSET, /* 0x2b GSET */
	uj_BC_TGETV, /* 0x2c TGETV */
	uj_BC_TGETS, /* 0x2d TGETS */
	uj_BC_TGETB, /* 0x2e TGETB */
	uj_BC_TSETV, /* 0x2f TSETV */
	uj_BC_TSETS, /* 0x30 TSETS */
	uj_BC_TSETB, /* 0x31 TSETB */
	uj_BC_TSETM, /* 0x32 TSETM */
	uj_BC_CALLM, /* 0x33 CALLM */
	uj_BC_CALL, /* 0x34 CALL */
	uj_BC_CALLMT, /* 0x35 CALLMT */
	uj_BC_CALLT, /* 0x36 CALLT */
	uj_BC_ITERC, /* 0x37 ITERC */
	uj_BC_ITERN, /* 0x38 ITERN */
	uj_BC_VARG, /* 0x39 VARG */
	uj_BC_ISNEXT, /* 0x3a ISNEXT */
	uj_BC_RETM, /* 0x3b RETM */
	uj_BC_RET, /* 0x3c RET */
	uj_BC_RET0, /* 0x3d RET0 */
	uj_BC_RET1, /* 0x3e RET1 */
	uj_BC_HOTCNT, /* 0x3f HOTCNT */
	uj_BC_COVERG, /* 0x40 COVERG */
	uj_BC_FORI, /* 0x41 FORI */
	uj_BC_NYI, /* 0x42 JFORI */
	uj_BC_IFORL, /* 0x43 FORL */
	uj_BC_IFORL, /* 0x44 IFORL */
	uj_BC_NYI, /* 0x45 JFORL */
	uj_BC_IITERL, /* 0x46 ITERL */
	uj_BC_IITERL, /* 0x47 IITERL */
	uj_BC_NYI, /* 0x48 JITERL */
	uj_BC_IITRNL, /* 0x49 ITRNL */
	uj_BC_IITRNL, /* 0x4a IITRNL */
	uj_BC_NYI, /* 0x4b JITRNL */
	uj_BC_ILOOP, /* 0x4c LOOP */
	uj_BC_ILOOP, /* 0x4d ILOOP */
	uj_BC_NYI, /* 0x4e JLOOP */
	uj_BC_JMP, /* 0x4f JMP */
	uj_BC_IFUNCF, /* 0x50 FUNCF */
	uj_BC_IFUNCF, /* 0x51 IFUNCF */
	uj_BC_NYI, /* 0x52 JFUNCF */
	uj_BC_IFUNCV, /* 0x53 FUNCV */
	uj_BC_IFUNCV, /* 0x54 IFUNCV */
	uj_BC_NYI, /* 0x55 JFUNCV */
	uj_BC_FUNCC, /* 0x56 FUNCC */
	uj_BC_FUNCCW, /* 0x57 FUNCCW */
	/* C interpreter fast functions */
	uj_ff_assert, /* 0x58, assert */
	uj_ff_type, /* 0x59, type */
	uj_ff_next, /* 0x5a, next */
	uj_ff_pairs, /* 0x5b, pairs */
	uj_ff_ipairs_aux, /* 0x5c, ipairs_aux */
	uj_ff_ipairs, /* 0x5d, ipairs */
	uj_ff_getmetatable, /* 0x5e, getmetatable */
	uj_ff_setmetatable, /* 0x5f, setmetatable */
	uj_ff_rawget, /* 0x60, rawget */
	uj_ff_tonumber, /* 0x61, tonumber */
	uj_ff_tostring, /* 0x62, tostring */
	uj_ff_pcall, /* 0x63, pcall */
	uj_ff_xpcall, /* 0x64, xpcall */
	/* coroutine library */
	uj_ff_coroutine_yield, /* 0x65, coroutine.yield */
	uj_ff_coroutine_resume, /* 0x66, coroutine.resume */
	uj_ff_coroutine_wrap_aux, /* 0x67, coroutine.wrap_aux */
	/* math library */
	uj_ff_math_fabs, /* 0x68, math.abs */
	uj_ff_math_floor, /* 0x69, math.floor */
	uj_ff_math_ceil, /* 0x6a, math.ceil */
	uj_ff_math_sqrt, /* 0x6b, math.sqrt */
	uj_ff_math_log10, /* 0x6c, math.log10 */
	uj_ff_math_exp, /* 0x6d, math.exp */
	uj_ff_math_sin, /* 0x6e, math.sin */
	uj_ff_math_cos, /* 0x6f, math.cos */
	uj_ff_math_tan, /* 0x70, math.tan */
	uj_ff_math_asin, /* 0x71, math.asin */
	uj_ff_math_acos, /* 0x72, math.acos */
	uj_ff_math_atan, /* 0x73, math.atan */
	uj_ff_math_sinh, /* 0x74, math.sinh */
	uj_ff_math_cosh, /* 0x75, math.cosh */
	uj_ff_math_tanh, /* 0x76, math.tanh */
	uj_ff_math_frexp, /* 0x77, math.frexp */
	uj_ff_math_modf, /* 0x78, math.modf */
	uj_ff_math_rad_deg, /* 0x79, math.deg */
	uj_ff_math_rad_deg, /* 0x7a, math.rad */
	uj_ff_math_log, /* 0x7b, math.log */
	uj_ff_math_atan2, /* 0x7c, math.atan2 */
	uj_ff_math_pow, /* 0x7d, math.pow */
	uj_ff_math_fmod, /* 0x7e, math.fmod */
	uj_ff_math_ldexp, /* 0x7f, math.ldexp */
	uj_ff_math_min, /* 0x80, math.min */
	uj_ff_math_max, /* 0x81, math.max */
	/* bit library */
	uj_ff_bit_tobit, /* 0x82, bit.tobit */
	uj_ff_bit_bnot, /* 0x83, bit.bnot */
	uj_ff_bit_bswap, /* 0x84, bit.bswap */
	uj_ff_bit_lshift, /* 0x85, bit.lshift */
	uj_ff_bit_rshift, /* 0x86, bit.rshift */
	uj_ff_bit_arshift, /* 0x87, bit.arshift */
	uj_ff_bit_rol, /* 0x88, bit.rol */
	uj_ff_bit_ror, /* 0x89, bit.ror */
	uj_ff_bit_band, /* 0x8a, bit.band */
	uj_ff_bit_bor, /* 0x8b, bit.bor */
	uj_ff_bit_bxor, /* 0x8c, bit.bxor */
	/* string library */
	uj_ff_string_len, /* 0x8d, string.len */
	uj_ff_string_byte, /* 0x8e, string.byte */
	uj_ff_string_char, /* 0x8f, string.char */
	uj_ff_string_sub, /* 0x90, string.sub */
	uj_ff_string_rep, /* 0x91, string.rep */
	uj_ff_string_reverse, /* 0x92, string.reverse */
	uj_ff_string_lower, /* 0x93, string.lower */
	uj_ff_string_upper, /* 0x94, string.upper */
	/* table library */
	uj_ff_table_getn, /* 0x95, table.getn */
};

LJ_STATIC_ASSERT((sizeof(uj_vm_bc_dispatch_cinterp) /
		  sizeof(uj_vm_bc_dispatch_cinterp[0])) == GG_LEN_DDISP);

static LJ_AINLINE void vm_check_cinterp_magic_num(const struct vm_frame *vmf)
{
	if ((vmf)->c_interp_marker != C_INTERPRETER_MAGIC_NUM)
		abort(); /* Integrity check failed */
}

/*
 * Every comparison bytecode is followed by JMP instruction.
 * So it is possible to read JMP target without actually
 * executing JMP bytecode.
 */
static LJ_AINLINE BCIns *jump_target(BCIns *pc)
{
	/* +1 to skip JMP itself */
	return pc + vm_rj(*(pc)) + 1;
}

#ifdef UJIT_CINTERP
void LJ_NOINLINE *uj_vm_cont_ra(HANDLER_SIGNATURE, TValue *results)
{
	*vm_slot_ra(base, *(pc - 1)) = *results;
	return DISPATCH();
}

void LJ_NOINLINE *uj_vm_cont_nop(HANDLER_SIGNATURE, TValue *results)
{
	return DISPATCH();
}

void LJ_NOINLINE *uj_vm_cont_condt(HANDLER_SIGNATURE, TValue *results)
{
	pc++;

	if (tvistruecond(results))
		pc += vm_rj(*(pc - 1));

	return DISPATCH();
}

void LJ_NOINLINE *uj_vm_cont_condf(HANDLER_SIGNATURE, TValue *results)
{
	pc++;

	if (!tvistruecond(results))
		pc += vm_rj(*(pc - 1));

	return DISPATCH();
}

void LJ_NOINLINE *uj_vm_cont_cat(HANDLER_SIGNATURE, TValue *results,
				 TValue *caller_base)
{
	TValue *src_start = vm_slot_rb(base, *(pc - 1));
	TValue *src_end = caller_base - 2;

	if (src_start == src_end) {
		*vm_slot_ra(base, *(pc - 1)) = *results;
		return DISPATCH();
	}

	*(caller_base - 2) = *results;
	return vm_bc_cat_z(HANDLER_ARGUMENTS, src_start, src_end);
}
#endif

static LJ_AINLINE void vm_check_timeout(struct vm_frame *vmf, TValue *base,
					BCIns *pc)
{
	lua_State *L = vmf->L;
	uint64_t nticks;

	if (L->timeout.usec == 0)
		return;

	if (((uintptr_t)L->cframe & CFRAME_RESUME) == 0)
		return;

	if (G(L)->hookmask & HOOK_ACTIVE)
		return;

	if (L->events & EXTEV_ANY_EVENT)
		return;

	nticks = uj_timerint_ticks();

	if (nticks < L->timeout.expticks)
		return; /* Cannot expire: has remaining ticks */

	/* Coroutine has expired */
	save_PC(pc);
	L->base = base;
	uj_throw_timeout(L);
}

static LJ_AINLINE void vm_check_immutable_bc(struct vm_frame *vmf, BCIns *pc,
					     TValue *base, GCobj *o)
{
	lua_State *L = vmf->L;

	if (LJ_LIKELY(!uj_obj_is_immutable(o)))
		return;

	save_PC(pc);
	L->base = base;
	uj_err(L, UJ_ERR_IMMUT_MODIF);
}

static LJ_AINLINE void vm_newtab_gccheck(struct vm_frame *vmf, TValue *base,
					 BCIns *pc)
{
	lua_State *L = vmf->L;

	save_PC(pc);
	L->base = base;
	lj_gc_check_fixtop(L);
}

static LJ_AINLINE void vm_copy_slots(TValue *dst, const TValue *src, size_t n)
{
	while (n) {
		*dst++ = *src++;
		n--;
	}
}

static LJ_AINLINE void vm_nil_slots(TValue *dst, size_t n)
{
	while (n)
		setnilV(dst + --n);
}

static LJ_AINLINE void vm_set_vmstate(lua_State *L, enum vmstate vmst)
{
	uj_vmstate_set(&G((L))->vmstate, vmst);
}

static LJ_AINLINE void vm_save_vmstate(lua_State *L, struct vm_frame *vmf)
{
	uj_vmstate_save(G((L))->vmstate, &vmf->vmsc);
}

static LJ_AINLINE void vm_restore_vmstate(lua_State *L, struct vm_frame *vmf)
{
	uj_vmstate_restore(&G((L))->vmstate, &vmf->vmsc);
}

static LJ_AINLINE void vm_set_atomic_vmstate(lua_State *L, TValue *base,
					     enum vmstate vmst)
{
	/* Guard for non-atomic VM context restoration */
	vm_set_vmstate(L, UJ_VMST_INTERP);
	G(L)->top_frame.guesttop.interp_base = base;
	vm_set_vmstate(L, vmst);
}

/*
 *-----------------------------------------------------------------------
 *-- Metamethod handling ------------------------------------------------
 *-----------------------------------------------------------------------
 */

static LJ_AINLINE int vm_tbl_check_mm(const GCtab *tab, const enum MMS mm)
{
	return tab->metatable != NULL && !(tab->metatable->nomm & (1u << mm));
}

/* -- Table indexing metamethods ----------------------------------------- */
static LJ_NOINLINE void *vm_vmeta_tget_handler(HANDLER_SIGNATURE, TValue *tab,
					       TValue *key)
{
	lua_State *L = vmf->L;
	const TValue *mtv;
	TValue *top;
	int64_t ftsz;

	save_PC(pc);

	L->base = base;
	mtv = uj_meta_tget(L, tab, key);
	base = L->base;

	/* TValue * (finished) or NULL (metamethod) returned */
	if (mtv != NULL) {
		TValue *dst = vm_slot_ra(base, ins);
		*dst = *mtv;
		return DISPATCH();
	}

	/* Call __index metamethod */
	top = L->top;
	ftsz = mm_call_ftsz(top, base);

	/* top-1 is further framelink, top-2 is continuation slot */
	setframe_pc(top - 2, pc); /* [cont|PC] */
	setframe_ftsz(top - 1, ftsz);

	pc = funcV(top - 1)->l.pc;
	base = top;

	return DISPATCH_CALL(2 + 1); /* 2 args for func(t, k) */
}

static LJ_NOINLINE void *vm_vmeta_tset_handler(HANDLER_SIGNATURE, TValue *tab,
					       TValue *key)
{
	lua_State *L = vmf->L;
	TValue *mtv, *top;
	int64_t ftsz;

	save_PC(pc);

	L->base = base;
	mtv = uj_meta_tset(L, tab, key);
	base = L->base;

	/* TValue * (finished) or NULL (metamethod) returned */
	if (mtv != NULL) {
		*mtv = *vm_slot_ra(base, ins);
		return DISPATCH();
	}

	/* Call __newindex metamethod */
	top = L->top;
	ftsz = mm_call_ftsz(top, base);

	/* top-1 is further framelink, top-2 is continuation slot */
	setframe_pc(top - 2, pc); /* [cont|PC] */
	*(top + 2) = *vm_slot_ra(base, ins); /* Copy value to third argument */
	setframe_ftsz(top - 1, ftsz);

	pc = funcV(top - 1)->l.pc;
	base = top;

	return DISPATCH_CALL(3 + 1); /* 3 args for func(t, k, v) */
}

static LJ_AINLINE void *vm_vmeta_tgets(HANDLER_SIGNATURE, TValue *tab,
				       GCstr *key)
{
	setstrV(vmf->L, &vmf->tmptv, key);
	return vm_vmeta_tget_handler(HANDLER_ARGUMENTS, tab, &vmf->tmptv);
}

static LJ_AINLINE void *vm_vmeta_tgetb(HANDLER_SIGNATURE, TValue *tab,
				       int32_t idx)
{
	setnumV(&vmf->tmptv, (double)idx);
	return vm_vmeta_tget_handler(HANDLER_ARGUMENTS, tab, &vmf->tmptv);
}

static LJ_AINLINE void *vm_vmeta_tsets(HANDLER_SIGNATURE, TValue *tab,
				       GCstr *key)
{
	setstrV(vmf->L, &vmf->tmptv, key);
	return vm_vmeta_tset_handler(HANDLER_ARGUMENTS, tab, &vmf->tmptv);
}

static LJ_AINLINE void *vm_vmeta_tsetb(HANDLER_SIGNATURE, TValue *tab,
				       int32_t idx)
{
	setnumV(&vmf->tmptv, (double)idx);
	return vm_vmeta_tset_handler(HANDLER_ARGUMENTS, tab, &vmf->tmptv);
}

static LJ_AINLINE TValue *vm_tbl_find_str_key(const GCtab *tab,
					      const GCstr *key)
{
	Node *n = &(tab->node[tab->hmask & key->hash]);

	do {
		if (tvisstr(&n->key) && strV(&n->key) == key)
			return &n->val;
	} while ((n = n->next));

	return NULL;
}

static void *vm_tgets_handler(HANDLER_SIGNATURE, TValue *tvtab, GCstr *key)
{
	GCtab *tab = tabV(tvtab);
	TValue *dst = vm_slot_ra(base, ins);
	TValue *found = vm_tbl_find_str_key(tab, key);

	if (found != NULL && !tvisnil(found)) {
		*dst = *found;
		return DISPATCH();
	}

	if (vm_tbl_check_mm(tab, MM_index))
		return vm_vmeta_tgets(HANDLER_ARGUMENTS, tvtab, key);

	setnilV(dst);

	return DISPATCH();
}

static void *vm_tgetb_handler(HANDLER_SIGNATURE, TValue *tvtab, int32_t idx)
{
	GCtab *tab = tabV(tvtab);
	TValue *dst = vm_slot_ra(base, ins);
	TValue *val;

	if (!inarray(tab, idx))
		return vm_vmeta_tgetb(HANDLER_ARGUMENTS, tvtab, idx);

	val = arrayslot(tab, idx);

	if (tvisnil(val)) {
		if (vm_tbl_check_mm(tab, MM_index))
			return vm_vmeta_tgetb(HANDLER_ARGUMENTS, tvtab, idx);

		setnilV(dst);
		return DISPATCH();
	}

	*dst = *val;

	return DISPATCH();
}

static void *vm_tsets_handler(HANDLER_SIGNATURE, TValue *tvtab, GCstr *key)
{
	GCtab *tab = tabV(tvtab);
	TValue *found;

	/* Clear metamethod cache */
	tab->nomm = 0;

	found = vm_tbl_find_str_key(tab, key);

	if (found != NULL) {
		if (tvisnil(found) && vm_tbl_check_mm(tab, MM_newindex))
			return vm_vmeta_tsets(HANDLER_ARGUMENTS, tvtab, key);
	} else {
		/* no key */
		if (vm_tbl_check_mm(tab, MM_newindex))
			return vm_vmeta_tsets(HANDLER_ARGUMENTS, tvtab, key);

		setstrV(vmf->L, &vmf->tmptv, key);

		save_PC(pc);

		vmf->L->base = base;
		found = lj_tab_newkey(vmf->L, tab, &vmf->tmptv);
		base = vmf->L->base;
	}

	*found = *vm_slot_ra(base, ins);
	lj_gc_anybarriert(vmf->L, tab);

	return DISPATCH();
}

static void *vm_tsetb_handler(HANDLER_SIGNATURE, TValue *tvtab, int32_t idx)
{
	GCtab *tab = tabV(tvtab);
	TValue *val;

	if (!inarray(tab, idx))
		return vm_vmeta_tsetb(HANDLER_ARGUMENTS, tvtab, idx);

	val = arrayslot(tab, idx);

	if ((tvisnil(val) && vm_tbl_check_mm(tab, MM_newindex)))
		return vm_vmeta_tsetb(HANDLER_ARGUMENTS, tvtab, idx);

	lj_gc_anybarriert(vmf->L, tab);
	*val = *vm_slot_ra(base, ins);

	return DISPATCH();
}

static LJ_AINLINE void *vm_vmeta_binop_handler(HANDLER_SIGNATURE, TValue *top)
{
	int64_t ftsz = mm_call_ftsz(top, base);

	/* top-1 is further framelink, top-2 is continuation slot */
	setframe_pc(top - 2, pc); /* [cont|PC] */
	/* 2 args for func(a, b) */
	return vm_call_dispatch((BCIns *)ftsz, base, vmf, ins, top, 2 + 1);
}

static LJ_NOINLINE void *vm_vmeta_arith_vv(HANDLER_SIGNATURE, TValue *op1,
					   TValue *op2)
{
	lua_State *L = vmf->L;
	BCReg op = bc_op(*(pc - 1));
	TValue *dst = vm_slot_ra(base, ins);
	TValue *mtv;

	save_PC(pc);

	L->base = base;
	mtv = uj_meta_arith(vmf->L, dst, op1, op2, op);
	base = L->base;

	if (mtv == NULL)
		return DISPATCH();

	return vm_vmeta_binop_handler(HANDLER_ARGUMENTS, mtv);
}

static LJ_NOINLINE void *vm_vmeta_comp_handler(HANDLER_SIGNATURE, TValue *op1,
					       TValue *op2)
{
	lua_State *L = vmf->L;
	BCReg op = bc_op(*(pc - 1));
	TValue *mtv;

	save_PC(pc);

	L->base = base;
	mtv = uj_meta_comp(L, op1, op2, op);
	base = L->base;

	if (mtv == NULL) {
		pc++; /* Skip JMP */
		return DISPATCH();
	} else if ((uintptr_t)mtv == 1) {
		pc = jump_target(pc);
		return DISPATCH();
	}

	return vm_vmeta_binop_handler(HANDLER_ARGUMENTS, mtv);
}

static LJ_NOINLINE void *vm_vmeta_equal_handler(HANDLER_SIGNATURE, TValue *op1,
						TValue *op2, int ne)
{
	lua_State *L = vmf->L;
	TValue *mtv;

	save_PC(pc);

	L->base = base;
	mtv = uj_meta_equal(L, gcval(op1), gcval(op2), ne);
	base = L->base;

	if (LJ_UNLIKELY((intptr_t)mtv == (intptr_t)ne)) {
		if (!ne)
			pc++; /* Skip JMP */
		else
			pc = jump_target(pc);

		return DISPATCH();
	}

	return vm_vmeta_binop_handler(pc, base, vmf, ins, mtv);
}

/*
 *-----------------------------------------------------------------------
 *-- Fast functions handling --------------------------------------------
 *-----------------------------------------------------------------------
 */

/* Set proper state for sampling profiler */
static LJ_AINLINE void vm_set_vmstate_ffunc(lua_State *L, TValue *base)
{
	vm_set_vmstate(L, UJ_VMST_INTERP);
	G(L)->top_frame.ffid = funcV(base - 1)->c.ffid;
	vm_set_vmstate(L, UJ_VMST_FFUNC);
}

static LJ_NOINLINE void *vm_fff_res_(HANDLER_SIGNATURE)
{
	uint8_t expected_nres1;

	if ((uintptr_t)pc & FRAME_TYPE)
		/* Negative TValue size */
		return vm_return(HANDLER_ARGUMENTS, -sizeof(TValue));

	/* nargs1 from BC_CALL */
	expected_nres1 = vm_raw_rb(*(pc - 1));
	/* More results expected? Clear missing return values */
	if (expected_nres1 > vmf->multres1) {
		/* Skip filled slots, starting from base-1 */
		TValue *start_from = base + vmf->multres1 - 2;
		size_t nslots = expected_nres1 - vmf->multres1;
		vm_nil_slots(start_from, nslots);
	}

	base = restore_base(base, pc);
	vm_set_atomic_vmstate(vmf->L, base, UJ_VMST_LFUNC);

	return DISPATCH();
}

static LJ_AINLINE void *vm_fff_res(HANDLER_SIGNATURE, uint64_t nargs1)
{
	vmf->multres1 = nargs1;
	return vm_fff_res_(HANDLER_ARGUMENTS);
}

static LJ_AINLINE void *vm_fff_res0(HANDLER_SIGNATURE)
{
	return vm_fff_res(HANDLER_ARGUMENTS, FFUNC_NARGS(0));
}

static LJ_AINLINE void *vm_fff_res1(HANDLER_SIGNATURE)
{
	return vm_fff_res(HANDLER_ARGUMENTS, FFUNC_NARGS(1));
}

static LJ_AINLINE void *vm_fff_res2(HANDLER_SIGNATURE)
{
	return vm_fff_res(HANDLER_ARGUMENTS, FFUNC_NARGS(2));
}

static LJ_AINLINE void *vm_call_dispatch(HANDLER_SIGNATURE, TValue *oldbase,
					 uint16_t nargs1)
{
	TValue *fn = oldbase - 1;

	if (LJ_UNLIKELY(!tvisfunc(fn)))
		nargs1 = vm_vmeta_call(HANDLER_ARGUMENTS, fn, nargs1);

	base = oldbase;
	fn->fr.tp.pcr = pc;
	pc = ((GCfuncL *)fn->gcr)->pc;

	return DISPATCH_CALL(nargs1);
}

/* Reconstruct previous base for vmeta_call during tailcall */
static void *vm_call_tail(HANDLER_SIGNATURE, uint16_t nargs1)
{
	TValue *oldbase = base;

	if ((uintptr_t)pc & FRAME_TYPE)
		base = (TValue *)((char *)base - ftsz2offs(pc));
	else
		base = restore_base(base, pc);

	return vm_call_dispatch(HANDLER_ARGUMENTS, oldbase, nargs1);
}

static LJ_AINLINE TValue *vm_ffgccheck(struct vm_frame *vmf, BCIns *pc,
				       uint16_t nargs1)
{
	lua_State *L = vmf->L;

	if (uj_mem_total(MEM(L)) < G(L)->gc.threshold)
		return L->base;

	save_PC(pc);
	/* fff_gcstep */
	L->top = L->base + nargs1 - 1;
	lj_gc_step(L);
	return L->base;
}

/*
 * This fallback is called by builtins that have a fallback function written
 * in C when it's impossible to complete fast path logic. After fallback
 * executes there are 3 possible outcomes: execute next bytecode, call
 * specific metamethod or try to execute builtin one more time. See uj_lib.h
 * for more information.
 */
static LJ_NOINLINE void *vm_fff_fallback(HANDLER_SIGNATURE, uint16_t nargs1)
{
	lua_State *L = vmf->L;
	GCfunc *fn;
	int nres;

	pc = restore_PC(base);
	save_PC(pc); /* Redundant (but a defined value) */

	L->base = base;
	L->top = base + nargs1 - 1;

	if (L->top + LUA_MINSTACK > L->maxstack) {
		/* Grow stack for fallback handler */
		uj_state_stack_grow(L, LUA_MINSTACK);
		base = L->base;
	}

	fn = funcV(base - 1);
	nres = fn->c.f(L);
	base = L->base;

	if (nres > FFH_RETRY) /* Returned nresults+1 */
		return vm_fff_res(HANDLER_ARGUMENTS, (uint64_t)nres);

	if (nres == FFH_TAILCALL)
		return vm_call_tail(HANDLER_ARGUMENTS, nargs1);

	fn = funcV(base - 1); /* Update if stack was reallocated */
	pc = fn->l.pc;

	/* Retry fast path. Pass nargs1 to prologue */
	return DISPATCH_CALL(nargs1);
}

static LJ_AINLINE void *vm_execute_first_bytecode(struct vm_frame *vmf,
						  TValue *base, int frame_type)
{
	const GCfunc *fn;
	int64_t ftsz;
	uint16_t nargs1;

	/* Coroutine about to be resumed: INTERP until executing BC_IFUNC* */
	vm_set_vmstate(vmf->L, UJ_VMST_INTERP);

	nargs1 = (uint16_t)(vmf->L->top - base) + 1; /* ptrdiff2nargs */
	ftsz = (int64_t)frame_type +
	       (int64_t)((uintptr_t)base - (uintptr_t)vmf->L->base);

	if (!tvisfunc(base - 1))
		nargs1 = vm_vmeta_call((BCIns *)(uintptr_t)ftsz, base, vmf,
				       0 /*ins*/, base - 1, nargs1);

	fn = funcV(base - 1);
	(base - 1)->fr.tp.ftsz = (int64_t)ftsz;
	return vm_next_call(proto_bc(funcproto(fn)), base, vmf, nargs1);
}

void *uj_vm_call_entry(lua_State *L, TValue *base, int nres1, ptrdiff_t ef,
		       int frame_type, struct vm_frame *vmf)
{
	/* SAVE_ERRF */
	vmf->errf = ef;
	/* SAVE_NRES */
	vmf->nres1 = nres1;
	/* SAVE_L */
	vmf->L = L;
	/* Add our C frame to cframe chain */
	vmf->cframe_prev = L->cframe;
	/* SAVE_PC (any value outside of bytecode is ok) */
	vmf->pc = (void *)L;
	L->cframe = (void *)(uintptr_t)vmf;
#if __x86_64__
	/* Save original return address */
	vmf->interp_ret_addr = *(uint64_t *)((ptrdiff_t)vmf - 8);
#endif
	vmf->c_interp_marker = C_INTERPRETER_MAGIC_NUM;

	vm_save_vmstate(vmf->L, vmf);

	return vm_execute_first_bytecode(vmf, base, frame_type);
}

void uj_vm_call(lua_State *L, TValue *base, int nres1)
{
	uj_vm_prepare_call(L, base, nres1, 0, FRAME_C);
}

int uj_vm_pcall(lua_State *L, TValue *base, int nres1, ptrdiff_t ef)
{
	return (ptrdiff_t)uj_vm_prepare_call(L, base, nres1, ef, FRAME_CP);
}

void *uj_vm_cpcall_entry(lua_State *L, lua_CFunction func, void *ud,
			 lua_CPFunction cp,
			 void __attribute__((unused)) * unused,
			 struct vm_frame *vmf)
{
	const GCfunc *fn;
	uint16_t nargs1;
	int64_t ftsz;
	TValue *base;

	vm_save_vmstate(L, vmf);

	vmf->c_interp_marker = C_INTERPRETER_MAGIC_NUM;
#if __x86_64__
	/* Save original return address */
	vmf->interp_ret_addr = *(uint64_t *)((ptrdiff_t)vmf - 8);
#endif
	/* SAVE_L */
	vmf->L = L;
	/* SAVE_PC (any value outside of bytecode is ok) */
	vmf->pc = (void *)L;
	/* No error function */
	vmf->errf = 0;
	/* Neg. delta means cframe w/o frame. */
	vmf->nres1 = -uj_state_stack_save(L, L->top);
	/* Add our C frame to cframe chain */
	vmf->cframe_prev = L->cframe;
	L->cframe = vmf;

	base = cp(L, func, ud);

	if (base == NULL) {
		L->cframe = vmf->cframe_prev; /* Restore previous C frame */
		vm_set_vmstate(L, UJ_VMST_INTERP);
		G(L)->top_frame.guesttop.interp_base = L->base;
		vm_restore_vmstate(L, vmf);
		vm_check_cinterp_magic_num(vmf);
		return 0;
	}

	nargs1 = (uint16_t)(L->top - base) + 1; /* ptrdiff2nargs */
	ftsz = FRAME_CP + (int64_t)((uintptr_t)base - (uintptr_t)vmf->L->base);

	if (!tvisfunc(base - 1))
		nargs1 = vm_vmeta_call((BCIns *)(uintptr_t)ftsz, base, vmf,
				       0 /*ins*/, base - 1, nargs1);

	fn = funcV(base - 1);
	(base - 1)->fr.tp.ftsz = (int64_t)ftsz;
	return vm_next_call(proto_bc(funcproto(fn)), base, vmf, nargs1);
}

int uj_vm_cpcall(lua_State *L, lua_CFunction func, void *ud, lua_CPFunction cp)
{
	return (int)(ptrdiff_t)uj_vm_prepare_cpcall(L, func, ud, cp);
}

static LJ_AINLINE void *vm_bc_cat_z(HANDLER_SIGNATURE, TValue *src_start,
				    TValue *src_end)
{
	lua_State *L = vmf->L;
	TValue *mtv, *dst;

	save_PC(pc);

	L->base = base;
	mtv = uj_meta_cat(vmf->L, src_start, src_end);
	base = L->base;

	if (mtv != NULL)
		return vm_vmeta_binop_handler(HANDLER_ARGUMENTS, mtv);

	lj_gc_check_fixtop(L);
	base = L->base;

	/* uj_meta_cat can trigger guest stack reallocation, so we need to update
	 * dst and src_start */
	dst = vm_slot_ra(base, *(pc - 1));
	src_start = vm_slot_rb(base, *(pc - 1));

	*dst = *src_start;

	return DISPATCH();
}

/* See BC_RET_Z label in original BC_RET bytecode */
void *bc_ret_z(HANDLER_SIGNATURE, TValue *results)
{
	uint8_t expected_nres1;

	/* Move results. For RET0 does nothing */
	vm_copy_slots(base - 1, results, vmf->multres1 - 1);

	/* pc-1 is BC_CALL* */
	expected_nres1 = vm_raw_rb(*(pc - 1));
	/* More results expected? Clear missing return values */
	if (expected_nres1 > vmf->multres1) {
		/* First slot after results */
		TValue *start_from = base + (vmf->multres1 - 1) - 1;
		size_t nslots = expected_nres1 - vmf->multres1;
		vm_nil_slots(start_from, nslots);
	}

	base = restore_base(base, pc);
	vmf->kbase = setup_kbase(base);

	vm_set_atomic_vmstate(vmf->L, base, UJ_VMST_LFUNC);
	vm_check_timeout(vmf, base, pc);

	/* NYI: uj_iprof_tick */

	return DISPATCH();
}

/* Reconstructs interpreter's state and transfers control to next bytecode after pcall/xpcall */
void *uj_vm_unwind_ff_eh(struct vm_frame *vmf)
{
	lua_State *L = vmf->L;
	TValue *base = L->base;
	BCIns *pc = restore_PC(base);

	setboolV(base - 1, 0); /* Set false to first result of pcall */
	vm_set_vmstate(vmf->L, UJ_VMST_INTERP);

	return vm_returnc(pc, base, vmf, 0 /*ins*/,
			  -sizeof(TValue) /*ra_offset*/, 1 + 1 /*nres*/);
}

/* Just exit from interpreter */
void *uj_vm_unwind_c_eh(struct vm_frame *vmf, int retcode)
{
	vm_set_vmstate(vmf->L, UJ_VMST_INTERP);
	G(vmf->L)->top_frame.guesttop.interp_base = vmf->L->base;
	vm_restore_vmstate(vmf->L, vmf);

	vm_check_cinterp_magic_num(vmf);
	return (void *)(ptrdiff_t)retcode;
}

static LJ_NOINLINE void *vm_cont_dispatch(HANDLER_SIGNATURE,
					  ptrdiff_t ra_offset)
{
	/* NYI: iprof check */
	TValue *results = (TValue *)((ptrdiff_t)base + ra_offset);
	TValue *caller_base = base;
	TValue *old_base = (TValue *)((char *)base - ftsz2offs(pc));
	ASMFunction contf;

	setnilV(results + vm_raw_rd(ins) - 1); /* set nil tag above results */
	pc = frame_pc(base - 2);

	if ((base - 2)->u32.lo <= LJ_CONT_FFI_CALLBACK)
		abort(); /* NYI: FFI */

	setnilV(base - 2); /* nil tag on continuation slot */

	base = old_base;
	vmf->kbase = pc2proto(base2func(base).l.pc)->k; /* setup_kbase */

	vm_set_atomic_vmstate(vmf->L, base, UJ_VMST_LFUNC);

	contf = frame_contf(caller_base - 1);
#if UJIT_CINTERP
#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-non-prototype"
#endif
	return contf(HANDLER_ARGUMENTS, results, caller_base);
#if __clang__
#pragma clang diagnostic pop
#endif
#else
	if (contf == lj_cont_ra) {
		goto cont_ra;
	} else if (contf == lj_cont_nop) {
		goto cont_nop;
	} else if (contf == lj_cont_condt) {
		goto cont_cond_true;
	} else if (contf == lj_cont_condf) {
		goto cont_cond_false;
	} else if (contf == lj_cont_cat) {
		goto cont_cat;
	} else {
		abort();
	}

cont_ra:
	*vm_slot_ra(base, *(pc - 1)) = *results;
	// fallthrough
cont_nop:
	return DISPATCH();

cont_cond_true:
	pc++;
	if (tvistruecond(results)) {
		pc += vm_rj(*(pc - 1));
	}
	return DISPATCH();

cont_cond_false:
	pc++;
	if (!tvistruecond(results)) {
		pc += vm_rj(*(pc - 1));
	}
	return DISPATCH();

cont_cat:; /* "label followed by a declaration is a C23 extension" */
	TValue *src_start = vm_slot_rb(base, *(pc - 1));
	TValue *src_end = caller_base - 2;

	if (src_start == src_end)
		goto cont_ra;

	*(caller_base - 2) = *results;
	return vm_bc_cat_z(HANDLER_ARGUMENTS, src_start, src_end);
#endif
}

static LJ_NOINLINE void *vm_returnp(HANDLER_SIGNATURE, uintptr_t ftsz,
				    ptrdiff_t ra_offset)
{
	TValue *results;

	if ((ftsz & FRAME_P) == 0)
		return vm_cont_dispatch(HANDLER_ARGUMENTS, ra_offset);

	/* Return from pcall or xpcall fast func */
	ftsz = ftsz2offs(ftsz);
	base = (TValue *)((ptrdiff_t)base - ftsz);
	ra_offset += ftsz - sizeof(TValue);
	results = (TValue *)((ptrdiff_t)base + ra_offset);
	pc = restore_PC(base);
	setboolV(results, 1);

	return vm_returnc(HANDLER_ARGUMENTS, ra_offset, vm_raw_rd(ins));
}

static LJ_NOINLINE void *vm_return(HANDLER_SIGNATURE, ptrdiff_t ra_offset)
{
	/*
	 * NB! Don't read RA operand from ins (i.e. vm_raw_ra(ins)) here - real RA
	 * can exceed 8 bits limit, use ra_offset instead.
	 */

	/* BASE = meta base, RA = resultofs, RD = nresults+1 (also in MULTRES) */
	uintptr_t ftsz = (uintptr_t)pc ^ FRAME_C;
	uint32_t nres1;
	TValue *results;

	if (ftsz & FRAME_TYPE)
		return vm_returnp(HANDLER_ARGUMENTS, ftsz, ra_offset);

	/* RD from BC_RET* bytecode added earlier */
	nres1 = vmf->multres1;
	lua_State *L = vmf->L;

	results = (TValue *)((ptrdiff_t)base + ra_offset);
	vm_copy_slots(base - 1, results, nres1 - 1);

	if (vmf->nres1 == 0) {
		/* 0 == LUA_MULTRET + 1 */
		/* Set stack top: */
		L->top = base + nres1 - 2;
	} else if (vmf->nres1 > nres1) {
		/* More results wanted. Check stack size and fill up results with nil. */
		int64_t n = vmf->nres1 - nres1;
		vm_nil_slots(base + nres1 - 2, n);
		L->top = base + nres1 - 1;
	} else {
		L->top = base + vmf->nres1 - 2;
	}

	/* Set stack base: */
	L->base = (TValue *)((char *)base - ftsz2offs(pc));

	/* NYI: uj_iprof_tick */

	vm_set_vmstate(vmf->L, UJ_VMST_INTERP);

	L->cframe = vmf->cframe_prev;
	vm_restore_vmstate(L, vmf);

	vm_check_cinterp_magic_num(vmf);
	return 0;
}

static void *vm_returnc(HANDLER_SIGNATURE, ptrdiff_t ra_offset, int nres)
{
	nres++;
	setbc_d_raw(&ins, nres);

	/*
	 * Seems that the only way when nres can be negative is when function
	 * called by FUNCC or FUNCCW returned negative result. There is no such
	 * test case in uJIT testing suite. If this assert will be triggered
	 * someday - please add implementation and corresponding test.
	 */
	if (nres == 0)
		abort(); /* Originally jumps to vm_unwind_yield */

	vmf->multres1 = nres;

	if (((uintptr_t)pc & FRAME_TYPE) == 0)
		return bc_ret_z(HANDLER_ARGUMENTS,
				(TValue *)((ptrdiff_t)base + ra_offset));

	return vm_return(HANDLER_ARGUMENTS, ra_offset);
}

void *uj_vm_callhook(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);
	CInterpFunction next_bc;

	save_PC(pc);

	L->base = base;
	L->top = base + nargs1 - 1;
	next_bc = uj_hook_call_cinterp(L, pc);
	base = L->base;

	vmf->pc = NULL;

	return next_bc(HANDLER_ARGUMENTS);
}

void *uj_vm_rethook(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;

	if (hook_active(L->glref))
		goto redispatch_static;

	L->base = base;
	uj_hook_ins(L, pc);
	base = L->base;

redispatch_static:
	return uj_vm_bc_dispatch_cinterp[bc_op(ins)](HANDLER_ARGUMENTS);
}

void *uj_vm_inshook(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;

	if (hook_active(L->glref))
		goto redispatch_static;

	if (!(L->glref->hookmask & (LUA_MASKLINE | LUA_MASKCOUNT)))
		goto redispatch_static;

	L->glref->hookcount--;

	if (L->glref->hookcount == 0 || L->glref->hookmask & LUA_MASKLINE) {
		L->base = base;
		uj_hook_ins(L, pc);
		base = L->base;
	}

redispatch_static:
	return uj_vm_bc_dispatch_cinterp[bc_op(ins)](HANDLER_ARGUMENTS);
}

static void *uj_BC_NYI(HANDLER_SIGNATURE)
{
	UNUSED(ins);
	UNUSED(pc);
	UNUSED(base);
	UNUSED(vmf);

	abort();
	return NULL;
}

static void *uj_BC_ISLT(HANDLER_SIGNATURE)
{
	/* RA = op1, RD = op2 */
	TValue *op1 = vm_slot_ra(base, ins);
	TValue *op2 = vm_slot_rd(base, ins);

	if (LJ_UNLIKELY(!tvisnum(op1) || !tvisnum(op2)))
		return vm_vmeta_comp_handler(HANDLER_ARGUMENTS, op1, op2);

	/* comparisons with NaN are always false */
	if (isnan(numV(op1)) || isnan(numV(op2))) {
		pc++; /* Skip JMP */
		return DISPATCH();
	}

	if (numV(op1) >= numV(op2))
		pc++; /* Skip JMP */
	else
		pc = jump_target(pc);

	return DISPATCH();
}

static void *uj_BC_ISGE(HANDLER_SIGNATURE)
{
	/* RA = op1, RD = op2 */
	TValue *op1 = vm_slot_ra(base, ins);
	TValue *op2 = vm_slot_rd(base, ins);

	if (LJ_UNLIKELY(!tvisnum(op1) || !tvisnum(op2)))
		return vm_vmeta_comp_handler(HANDLER_ARGUMENTS, op1, op2);

	if (numV(op1) < numV(op2))
		pc++; /* Skip JMP */
	else
		pc = jump_target(pc);

	return DISPATCH();
}

static void *uj_BC_ISLE(HANDLER_SIGNATURE)
{
	/* RA = op1, RD = op2 */
	TValue *op1 = vm_slot_ra(base, ins);
	TValue *op2 = vm_slot_rd(base, ins);

	if (LJ_UNLIKELY(!tvisnum(op1) || !tvisnum(op2)))
		return vm_vmeta_comp_handler(HANDLER_ARGUMENTS, op1, op2);

	/* comparisons with NaN are always false */
	if (isnan(numV(op1)) || isnan(numV(op2))) {
		pc++; /* Skip JMP */
		return DISPATCH();
	}

	if (numV(op1) > numV(op2))
		pc++; /* Skip JMP */
	else
		pc = jump_target(pc);

	return DISPATCH();
}

static void *uj_BC_ISGT(HANDLER_SIGNATURE)
{
	/* RA = op1, RD = op2 */
	TValue *op1 = vm_slot_ra(base, ins);
	TValue *op2 = vm_slot_rd(base, ins);

	if (LJ_UNLIKELY(!tvisnum(op1) || !tvisnum(op2)))
		return vm_vmeta_comp_handler(HANDLER_ARGUMENTS, op1, op2);

	if (numV(op1) <= numV(op2))
		pc++; /* Skip JMP */
	else
		pc = jump_target(pc);

	return DISPATCH();
}

/* Common handler for ISEQV and ISNEV */
static LJ_AINLINE void *vm_bc_eq_common_handler(HANDLER_SIGNATURE, int ne)
{
	/* RA = op1, RD = op2 */
	TValue *op1 = vm_slot_ra(base, ins);
	TValue *op2 = vm_slot_rd(base, ins);
	uint32_t tag1, tag2;

#if LJ_HASFFI
	if (LJ_UNLIKELY(tviscdata(op1)))
		abort(); /* NYI: FFI */
#endif

	if (tvisnum(op2) && tvisnum(op1)) {
		if (numV(op2) != numV(op1))
			goto iseqne_end_2;
		else
			goto iseqne_end_1;
	}

	tag1 = gettag(op1);
	tag2 = gettag(op2);

	if (tag1 != tag2) /* Not the same type? */
		goto iseqne_end_2;

	if (tvispri(op2)) /* Same type and primitive type? */
		goto iseqne_end_1;

	/* Same types and not a primitive type. Compare GCobj or pvalue. */

	if (gcval(op1) == gcval(op2)) /* Same GCobjs or pvalues? */
		goto iseqne_end_1;

	if (tag2 > LJ_TISTABUD) /* Different objects and not table/ud? */
		goto iseqne_end_2;

	if (tag2 < LJ_TUDATA) /* And not 64 bit lightuserdata */
		goto iseqne_end_2;

	if (!(vm_tbl_check_mm(tabV(op1), MM_eq))) /* no __eq */
		goto iseqne_end_2;

	/* Different tables or userdatas. Need to check __eq metamethod. */
	return vm_vmeta_equal_handler(HANDLER_ARGUMENTS, op1, op2, ne);

iseqne_end_1: /* corresponds to "$JMP_INSTR <1" in iseqne_end */
	if (ne)
		pc++; /* Skip JMP */
	else
		pc = jump_target(pc);

	return DISPATCH();

iseqne_end_2: /* corresponds to "$JMP_INSTR <2" in iseqne_end */
	if (!ne)
		pc++; /* Skip JMP */
	else
		pc = jump_target(pc);

	return DISPATCH();
}

static void *uj_BC_ISEQV(HANDLER_SIGNATURE)
{
	/* RA = op1, RD = op2 */
	return vm_bc_eq_common_handler(HANDLER_ARGUMENTS, 0);
}

static void *uj_BC_ISNEV(HANDLER_SIGNATURE)
{
	/* RA = op1, RD = op2 */
	return vm_bc_eq_common_handler(HANDLER_ARGUMENTS, 1);
}

static void *uj_BC_ISEQS(HANDLER_SIGNATURE)
{
	/* RA = src, RD = str const (~), JMP with RD = target */
	TValue *src = vm_slot_ra(base, ins);
	GCstr *str = (GCstr *)vm_kbase_gco(vmf->kbase, vm_raw_rd(ins));

#if LJ_HASFFI
	if (LJ_UNLIKELY(tviscdata(src)))
		abort(); /* NYI: FFI */
#endif

	if (!tvisstr(src) || strV(src) != str)
		pc++; /* Skip JMP */
	else
		pc = jump_target(pc);

	return DISPATCH();
}

static void *uj_BC_ISNES(HANDLER_SIGNATURE)
{
	/* RA = src, RD = str const (~), JMP with RD = target */
	TValue *src = vm_slot_ra(base, ins);
	GCstr *str = (GCstr *)vm_kbase_gco(vmf->kbase, vm_raw_rd(ins));

#if LJ_HASFFI
	if (LJ_UNLIKELY(tviscdata(src)))
		abort(); /* NYI: FFI */
#endif

	if (tvisstr(src) && strV(src) == str)
		pc++; /* Skip JMP */
	else
		pc = jump_target(pc);

	return DISPATCH();
}

static void *uj_BC_ISEQN(HANDLER_SIGNATURE)
{
	/* RA = src, RD = num const, JMP with RD = target */
	TValue *src = vm_slot_ra(base, ins);
	TValue *num = vm_slot_rd(vmf->kbase, ins);

#if LJ_HASFFI
	if (LJ_UNLIKELY(tviscdata(src)))
		abort(); /* NYI: FFI */
#endif

	if (!tvisnum(src) || numV(num) != numV(src))
		pc++; /* Skip JMP */
	else
		pc = jump_target(pc);

	return DISPATCH();
}

static void *uj_BC_ISNEN(HANDLER_SIGNATURE)
{
	/* RA = src, RD = num const, JMP with RD = target */
	TValue *src = vm_slot_ra(base, ins);
	TValue *num = vm_slot_rd(vmf->kbase, ins);

#if LJ_HASFFI
	if (LJ_UNLIKELY(tviscdata(src)))
		abort(); /* NYI: FFI */
#endif

	if (tvisnum(src) && numV(num) == numV(src))
		pc++; /* Skip JMP */
	else
		pc = jump_target(pc);

	return DISPATCH();
}

static void *uj_BC_ISEQP(HANDLER_SIGNATURE)
{
	/* RA = src, RD = primitive type (~), JMP with RD = target */
	TValue *src = vm_slot_ra(base, ins);
	uint32_t tag = ~vm_raw_rd(ins);

#if LJ_HASFFI
	if (LJ_UNLIKELY(tviscdata(src)))
		abort(); /* NYI: FFI */
#endif

	if (gettag(src) != tag)
		pc++; /* Skip JMP */
	else
		pc = jump_target(pc);

	return DISPATCH();
}

static void *uj_BC_ISNEP(HANDLER_SIGNATURE)
{
	/* RA = src, RD = primitive type (~), JMP with RD = target */
	TValue *src = vm_slot_ra(base, ins);
	uint32_t tag = ~vm_raw_rd(ins);

#if LJ_HASFFI
	if (LJ_UNLIKELY(tviscdata(src)))
		abort(); /* NYI: FFI */
#endif

	if (gettag(src) == tag)
		pc++; /* Skip JMP */
	else
		pc = jump_target(pc);

	return DISPATCH();
}

static void *uj_BC_ISTC(HANDLER_SIGNATURE)
{
	TValue *dst = vm_slot_ra(base, ins);
	TValue *src = vm_slot_rd(base, ins);

	if (LJ_UNLIKELY(!tvistruecond(src))) {
		pc++; /* Skip JMP */
	} else {
		*dst = *src;
		pc = jump_target(pc);
	}

	return DISPATCH();
}

static void *uj_BC_ISFC(HANDLER_SIGNATURE)
{
	TValue *dst = vm_slot_ra(base, ins);
	TValue *src = vm_slot_rd(base, ins);

	if (LJ_UNLIKELY(tvistruecond(src))) {
		pc++; /* Skip JMP */
	} else {
		*dst = *src;
		pc = jump_target(pc);
	}

	return DISPATCH();
}

static void *uj_BC_IST(HANDLER_SIGNATURE)
{
	TValue *test = vm_slot_rd(base, ins);

	if (!tvistruecond(test))
		pc++; /* Skip JMP */
	else
		pc = jump_target(pc);

	return DISPATCH();
}

static void *uj_BC_ISF(HANDLER_SIGNATURE)
{
	TValue *test = vm_slot_rd(base, ins);

	if (tvistruecond(test))
		pc++; /* Skip JMP */
	else
		pc = jump_target(pc);

	return DISPATCH();
}

static void *uj_BC_MOV(HANDLER_SIGNATURE)
{
	TValue *dst = vm_slot_ra(base, ins);
	TValue *src = vm_slot_rd(base, ins);

	*dst = *src;

	return DISPATCH();
}

static void *uj_BC_NOT(HANDLER_SIGNATURE)
{
	TValue *dst = vm_slot_ra(base, ins);
	TValue *op1 = vm_slot_rd(base, ins);

	setboolV(dst, !tvistruecond(op1));

	return DISPATCH();
}

static void *uj_BC_UNM(HANDLER_SIGNATURE)
{
	/* RA = dst, RD = src */
	TValue *dst = vm_slot_ra(base, ins);
	TValue *op1 = vm_slot_rd(base, ins);

	if (LJ_UNLIKELY(!tvisnum(op1)))
		return vm_vmeta_arith_vv(HANDLER_ARGUMENTS, op1, op1);

	setnumV(dst, -numV(op1));

	return DISPATCH();
}

static LJ_NOINLINE void *vm_vmeta_len(HANDLER_SIGNATURE, TValue *obj)
{
	lua_State *L = vmf->L;
	TValue *mtv;

	save_PC(pc);

	L->base = base;
	mtv = uj_meta_len(L, obj);
	base = L->base;

#if LJ_52
	if (mtv == NULL) {
		setnumV(vm_slot_ra(base, ins),
			lj_tab_len(tabV(vm_slot_rd(base, ins))));
		return DISPATCH();
	}
#endif
	return vm_vmeta_binop_handler(HANDLER_ARGUMENTS, mtv);
}

static void *uj_BC_LEN(HANDLER_SIGNATURE)
{
	/* RA = dst, RD = src */
	TValue *dst = vm_slot_ra(base, ins);
	TValue *obj = vm_slot_rd(base, ins);

	if (tvisstr(obj)) {
		setnumV(dst, strV(obj)->len);
		return DISPATCH();
	}

	if (tvistab(obj)) {
#if LJ_52
		if (vm_tbl_check_mm(tabV(obj), MM_len))
			return vm_vmeta_len(HANDLER_ARGUMENTS, obj);
#endif
		setnumV(dst, lj_tab_len(tabV(obj)));
		return DISPATCH();
	}

	return vm_vmeta_len(HANDLER_ARGUMENTS, obj);
}

static void *uj_BC_ADD(HANDLER_SIGNATURE)
{
	TValue *dst = vm_slot_ra(base, ins);
	TValue *op1 = vm_slot_rb(base, ins);
	TValue *op2 = vm_slot_rc(base, ins);

	if (LJ_UNLIKELY(!tvisnum(op1) || !tvisnum(op2)))
		return vm_vmeta_arith_vv(HANDLER_ARGUMENTS, op1, op2);

	setnumV(dst, numV(op1) + numV(op2));
	return DISPATCH();
}

static void *uj_BC_SUB(HANDLER_SIGNATURE)
{
	TValue *dst = vm_slot_ra(base, ins);
	TValue *op1 = vm_slot_rb(base, ins);
	TValue *op2 = vm_slot_rc(base, ins);

	if (LJ_UNLIKELY(!tvisnum(op1) || !tvisnum(op2)))
		return vm_vmeta_arith_vv(HANDLER_ARGUMENTS, op1, op2);

	setnumV(dst, numV(op1) - numV(op2));
	return DISPATCH();
}

static void *uj_BC_MUL(HANDLER_SIGNATURE)
{
	TValue *dst = vm_slot_ra(base, ins);
	TValue *op1 = vm_slot_rb(base, ins);
	TValue *op2 = vm_slot_rc(base, ins);

	if (LJ_UNLIKELY(!tvisnum(op1) || !tvisnum(op2)))
		return vm_vmeta_arith_vv(HANDLER_ARGUMENTS, op1, op2);

	setnumV(dst, numV(op1) * numV(op2));
	return DISPATCH();
}

static void *uj_BC_DIV(HANDLER_SIGNATURE)
{
	TValue *dst = vm_slot_ra(base, ins);
	TValue *op1 = vm_slot_rb(base, ins);
	TValue *op2 = vm_slot_rc(base, ins);

	if (LJ_UNLIKELY(!tvisnum(op1) || !tvisnum(op2)))
		return vm_vmeta_arith_vv(HANDLER_ARGUMENTS, op1, op2);

	setnumV(dst, numV(op1) / numV(op2));
	return DISPATCH();
}

/* copied from utils/uj_vmmath.c */
static LJ_AINLINE double vm_mod(double a, double b)
{
	/* according to Lua Reference */
	return a - floor(a / b) * b;
}

static void *uj_BC_MOD(HANDLER_SIGNATURE)
{
	TValue *dst = vm_slot_ra(base, ins);
	TValue *op1 = vm_slot_rb(base, ins);
	TValue *op2 = vm_slot_rc(base, ins);

	if (LJ_UNLIKELY(!tvisnum(op1) || !tvisnum(op2)))
		return vm_vmeta_arith_vv(HANDLER_ARGUMENTS, op1, op2);

	setnumV(dst, vm_mod(numV(op1), numV(op2)));
	return DISPATCH();
}

/* copied from utils/uj_vmmath.c */
static LJ_AINLINE double vm_pow(double x, double exp)
{
	return pow(x, exp);
}

static void *uj_BC_POW(HANDLER_SIGNATURE)
{
	TValue *dst = vm_slot_ra(base, ins);
	TValue *op1 = vm_slot_rb(base, ins);
	TValue *op2 = vm_slot_rc(base, ins);

	if (LJ_UNLIKELY(!tvisnum(op1) || !tvisnum(op2)))
		return vm_vmeta_arith_vv(HANDLER_ARGUMENTS, op1, op2);

	setnumV(dst, vm_pow(numV(op1), numV(op2)));
	return DISPATCH();
}

static void *uj_BC_CAT(HANDLER_SIGNATURE)
{
	/* RA = dst, RB = src_start, RC = src_end */
	TValue *src_start = vm_slot_rb(base, ins);
	TValue *src_end = vm_slot_rc(base, ins);
	return vm_bc_cat_z(HANDLER_ARGUMENTS, src_start, src_end);
}

static void *uj_BC_KSTR(HANDLER_SIGNATURE)
{
	TValue *dst = vm_slot_ra(base, ins);
	GCstr *str = (GCstr *)vm_kbase_gco(vmf->kbase, vm_raw_rd(ins));

	setstrV(vmf->L, dst, str);

	return DISPATCH();
}

static void *uj_BC_KCDATA(HANDLER_SIGNATURE)
{
	TValue *dst = vm_slot_ra(base, ins);
	GCcdata *cdata = (GCcdata *)vm_kbase_gco(vmf->kbase, vm_raw_rd(ins));

	setcdataV(vmf->L, dst, cdata);

	return DISPATCH();
}

static void *uj_BC_KSHORT(HANDLER_SIGNATURE)
{
	TValue *dst = vm_slot_ra(base, ins);
	int16_t i16 = (int16_t)vm_raw_rd(ins);

	setnumV(dst, (double)i16);

	return DISPATCH();
}

static void *uj_BC_KNUM(HANDLER_SIGNATURE)
{
	TValue *dst = vm_slot_ra(base, ins);
	TValue *src = vm_slot_rd(vmf->kbase, ins);

	*dst = *src;

	return DISPATCH();
}

static void *uj_BC_KPRI(HANDLER_SIGNATURE)
{
	TValue *dst = vm_slot_ra(base, ins);
	uint32_t tag = ~vm_raw_rd(ins);

	settag(dst, tag);

	return DISPATCH();
}

static void *uj_BC_KNIL(HANDLER_SIGNATURE)
{
	TValue *begin = vm_slot_ra(base, ins);
	TValue *end = vm_slot_rd(base, ins);

	for (; begin <= end; begin++)
		setnilV(begin);

	return DISPATCH();
}

static void *uj_BC_UGET(HANDLER_SIGNATURE)
{
	TValue *dst = vm_slot_ra(base, ins);
	GCupval *uv = vm_base_upval(base, vm_raw_rd(ins));

	*dst = *uvval(uv);

	return DISPATCH();
}

static void *uj_BC_USETV(HANDLER_SIGNATURE)
{
	GCupval *uv = vm_base_upval(base, vm_raw_ra(ins));
	TValue *src = vm_slot_rd(base, ins);

	*uvval(uv) = *src;

	if (LJ_UNLIKELY(uv->closed && isblack(obj2gco(uv)) && tvisgcv(src) &&
			iswhite(gcval(src)))) {
		/*
		 * No L->base and L->top sync is necessary
		 * since lj_gc_barrieruv doesn't affect Lua stack.
		 */
		lj_gc_barrieruv(G(vmf->L), uvval(uv));
	}

	return DISPATCH();
}

static void *uj_BC_USETS(HANDLER_SIGNATURE)
{
	GCupval *uv = vm_base_upval(base, vm_raw_ra(ins));
	GCstr *str = (GCstr *)vm_kbase_gco(vmf->kbase, vm_raw_rd(ins));

	setstrV(vmf->L, uvval(uv), str);

	if (LJ_UNLIKELY(isblack(obj2gco(uv)) && iswhite(obj2gco(str)) &&
			uv->closed)) {
		/*
		 * No L->base and L->top sync is necessary
		 * since lj_gc_barrieruv doesn't affect Lua stack.
		 */
		lj_gc_barrieruv(G(vmf->L), uvval(uv));
	}

	return DISPATCH();
}

static void *uj_BC_USETN(HANDLER_SIGNATURE)
{
	GCupval *uv = vm_base_upval(base, vm_raw_ra(ins));
	TValue *src = vm_slot_rd(vmf->kbase, ins);

	setnumV(uvval(uv), numV(src));

	return DISPATCH();
}

static void *uj_BC_USETP(HANDLER_SIGNATURE)
{
	GCupval *uv = vm_base_upval(base, vm_raw_ra(ins));
	uint32_t tag = ~vm_raw_rd(ins);

	settag(uvval(uv), tag);

	return DISPATCH();
}

static void *uj_BC_UCLO(HANDLER_SIGNATURE)
{
	TValue *level = vm_slot_ra(base, ins);

	pc += vm_rj(ins);

	if (LJ_LIKELY(vmf->L->openupval)) {
		/*
		 * No L->base and L->top sync is necessary
		 * since uj_upval_close doesn't affect Lua stack.
		 */
		uj_upval_close(vmf->L, level);
	}

	return DISPATCH();
}

static void *uj_BC_FNEW(HANDLER_SIGNATURE)
{
	/* RA = dst, RD = proto const (~) (holding function prototype) */
	GCproto *pt = (GCproto *)vm_kbase_gco(vmf->kbase, vm_raw_rd(ins));
	GCfuncL *parent = &base2func(base).l;
	TValue *dst;
	GCfunc *func;

	save_PC(pc);

	vmf->L->base = base;
	func = uj_func_newL_gc(vmf->L, pt, parent);
	base = vmf->L->base;

	dst = vm_slot_ra(base, ins);
	setfuncV(vmf->L, dst, func);

	return DISPATCH();
}

static void *uj_BC_TNEW(HANDLER_SIGNATURE)
{
	/* RA = dst, RD = hbits|asize */
	uint16_t rd = (uint16_t)vm_raw_rd(ins);
	uint32_t asize = rd & 0x7ff;
	uint32_t hbits = rd >> 11;
	GCtab *newTab;
	TValue *dst;

	if (LJ_UNLIKELY(asize == 0x7ff))
		asize = 0x801;

	/* Base synced in vm_newtab_gccheck */
	vm_newtab_gccheck(vmf, base, pc);
	newTab = lj_tab_new(vmf->L, asize, hbits);
	base = vmf->L->base;

	dst = vm_slot_ra(base, ins);
	settabV(vmf->L, dst, newTab);

	return DISPATCH();
}

static void *uj_BC_TDUP(HANDLER_SIGNATURE)
{
	/* RA = dst, RD = table const (~) (holding template table) */
	GCtab *src = (GCtab *)vm_kbase_gco(vmf->kbase, vm_raw_rd(ins));
	GCtab *copy;
	TValue *dst;

	/* Base synced in vm_newtab_gccheck */
	vm_newtab_gccheck(vmf, base, pc);
	copy = lj_tab_dup(vmf->L, src);
	base = vmf->L->base;

	dst = vm_slot_ra(base, ins);
	settabV(vmf->L, dst, copy);

	return DISPATCH();
}

static void *uj_BC_GGET(HANDLER_SIGNATURE)
{
	/* RA = dst, RD = str const (~) */
	GCtab *tab = base2func(base).l.env;
	GCstr *str = (GCstr *)vm_kbase_gco(vmf->kbase, vm_raw_rd(ins));
	TValue *tvtab = &G(vmf->L)->tmptv;

	settabV(vmf->L, tvtab, tab);
	return vm_tgets_handler(HANDLER_ARGUMENTS, tvtab, str);
}

static void *uj_BC_GSET(HANDLER_SIGNATURE)
{
	/* RA = src, RD = str const (~) */
	GCtab *tab = base2func(base).l.env;
	GCstr *key = (GCstr *)vm_kbase_gco(vmf->kbase, vm_raw_rd(ins));
	TValue *tvtab;

	vm_check_immutable_bc(vmf, pc, base, obj2gco(tab));

	tvtab = &G(vmf->L)->tmptv;
	settabV(vmf->L, tvtab, tab);
	return vm_tsets_handler(HANDLER_ARGUMENTS, tvtab, key);
}

static void *uj_BC_TGETV(HANDLER_SIGNATURE)
{
	/* RA = dst, RB = table, RC = key */
	TValue *tvkey = vm_slot_rc(base, ins);
	TValue *tvtab = vm_slot_rb(base, ins);

	if (LJ_UNLIKELY(!tvistab(tvtab)))
		return vm_vmeta_tget_handler(HANDLER_ARGUMENTS, tvtab, tvkey);

	if (tvisstr(tvkey)) {
		GCstr *key = strV(tvkey);
		return vm_tgets_handler(HANDLER_ARGUMENTS, tvtab, key);
	}

	if (tvisnum(tvkey)) {
		lua_Number nk = numV(tvkey);
		int32_t k = lj_num2int(nk);

		if (nk == (lua_Number)k) {
			return vm_tgetb_handler(HANDLER_ARGUMENTS, tvtab, k);
		}
		/* Fallthrough, generic numeric key */
	}

	return vm_vmeta_tget_handler(HANDLER_ARGUMENTS, tvtab, tvkey);
}

static void *uj_BC_TGETS(HANDLER_SIGNATURE)
{
	/* RA = dst, RB = table, RC = str const (~) */
	TValue *tvtab = vm_slot_rb(base, ins);
	GCstr *key = (GCstr *)vm_kbase_gco(vmf->kbase, vm_raw_rc(ins));

	if (LJ_UNLIKELY(!tvistab(tvtab)))
		return vm_vmeta_tgets(HANDLER_ARGUMENTS, tvtab, key);

	return vm_tgets_handler(HANDLER_ARGUMENTS, tvtab, key);
}

static void *uj_BC_TGETB(HANDLER_SIGNATURE)
{
	/* RA = dst, RB = table, RC = byte literal */
	TValue *tvtab = vm_slot_rb(base, ins);
	int32_t idx = (int32_t)vm_raw_rc(ins);

	if (LJ_UNLIKELY(!tvistab(tvtab)))
		return vm_vmeta_tgetb(HANDLER_ARGUMENTS, tvtab, idx);

	return vm_tgetb_handler(HANDLER_ARGUMENTS, tvtab, idx);
}

static void *uj_BC_TSETV(HANDLER_SIGNATURE)
{
	/* RA = src, RB = table, RC = key */
	TValue *tvtab = vm_slot_rb(base, ins);
	TValue *tvkey = vm_slot_rc(base, ins);

	if (LJ_UNLIKELY(!tvistab(tvtab)))
		return vm_vmeta_tset_handler(HANDLER_ARGUMENTS, tvtab, tvkey);

	vm_check_immutable_bc(vmf, pc, base, obj2gco(tabV(tvtab)));

	if (tvisstr(tvkey)) {
		GCstr *key = strV(tvkey);
		return vm_tsets_handler(HANDLER_ARGUMENTS, tvtab, key);
	}

	if (tvisnum(tvkey)) {
		lua_Number nk = numV(tvkey);
		int32_t k = lj_num2int(nk);

		if (nk == (lua_Number)k)
			return vm_tsetb_handler(HANDLER_ARGUMENTS, tvtab, k);
		else
			/* Generic numeric key */
			return vm_vmeta_tset_handler(HANDLER_ARGUMENTS, tvtab,
						     tvkey);
	}

	return vm_vmeta_tset_handler(HANDLER_ARGUMENTS, tvtab, tvkey);
}

static void *uj_BC_TSETS(HANDLER_SIGNATURE)
{
	/* RA = src, RB = table, RC = str const (~) */
	TValue *tvtab = vm_slot_rb(base, ins);
	GCstr *key = (GCstr *)vm_kbase_gco(vmf->kbase, vm_raw_rc(ins));

	if (LJ_UNLIKELY(!tvistab(tvtab)))
		return vm_vmeta_tsets(HANDLER_ARGUMENTS, tvtab, key);

	vm_check_immutable_bc(vmf, pc, base, obj2gco(tabV(tvtab)));

	return vm_tsets_handler(HANDLER_ARGUMENTS, tvtab, key);
}

static void *uj_BC_TSETB(HANDLER_SIGNATURE)
{
	/* RA = src, RB = table, RC = byte literal */
	TValue *tvtab = vm_slot_rb(base, ins);
	int32_t idx = (int32_t)vm_raw_rc(ins);

	if (LJ_UNLIKELY(!tvistab(tvtab)))
		return vm_vmeta_tsetb(HANDLER_ARGUMENTS, tvtab, idx);

	vm_check_immutable_bc(vmf, pc, base, obj2gco(tabV(tvtab)));

	return vm_tsetb_handler(HANDLER_ARGUMENTS, tvtab, idx);
}

static void *uj_BC_TSETM(HANDLER_SIGNATURE)
{
	/* RA = base (table at base-1), RD = num const (start index) */
	uint64_t startIdx = ((TValue *)vm_slot_rd(vmf->kbase, ins))->u32.lo;
	TValue *src = vm_slot_ra(base, ins);
	GCtab *tab = tabV(src - 1);
	uint64_t maxIdx;

	vm_check_immutable_bc(vmf, pc, base, obj2gco(tab));
	lj_gc_anybarriert(vmf->L, tab);

	if (vmf->multres1 == 1) /* Nothing to copy */
		return DISPATCH();

	maxIdx = startIdx + vmf->multres1 - 1;
	if (maxIdx > tab->asize) {
		save_PC(pc);

		vmf->L->base = base;
		lj_tab_reasize(vmf->L, tab, maxIdx);
		base = vmf->L->base;
		/*
		 * Probably redundant call, but original bytecode makes
		 * it after array reallocation.
		 */
		lj_gc_anybarriert(vmf->L, tab);
		/* Update in case of stack reallocation */
		src = vm_slot_ra(base, ins);
	}

	for (uint64_t i = 0; i < vmf->multres1 - 1; i++) {
		uint64_t key = startIdx + i;
		TValue *val = arrayslot(tab, key);
		*val = *(src + i);
	}

	return DISPATCH();
}

static LJ_AINLINE uint16_t vm_vmeta_call(HANDLER_SIGNATURE, TValue *fn,
					 uint16_t nargs1)
{
	save_PC(pc);

	vmf->L->base = base;
	uj_meta_call(vmf->L, fn, fn + nargs1);
	/*
	 * NOTE: syncing base below is questionable. Original interpreter does it,
	 * but seems that uj_meta_call can't affect L->base. In original vmeta_call
	 * there is a trick with BASE and KBASE to distinguish regular call from tail
	 * call which probably will not work if uj_meta_call can change BASE. Feel
	 * free to remove this comment and add base syncing here later.
	 */

	/* +1 since there will be one additional argument to __call - object itself */
	nargs1++;
	return nargs1;
}

static LJ_AINLINE void *vm_bc_call_regular_handler(HANDLER_SIGNATURE,
						   uint16_t nargs1)
{
	TValue *fn = vm_slot_ra(base, ins);

	vm_set_vmstate(vmf->L,
		       UJ_VMST_INTERP); /* INTERP until a new BASE is setup */

	if (LJ_UNLIKELY(!tvisfunc(fn)))
		nargs1 = vm_vmeta_call(HANDLER_ARGUMENTS, fn, nargs1);

	base = fn + 1;
	fn->fr.tp.pcr = pc;
	pc = ((GCfuncL *)fn->gcr)->pc;

	return DISPATCH_CALL(nargs1);
}

static void *uj_BC_CALLM(HANDLER_SIGNATURE)
{
	/* RA = base, (RB = nresults+1,) RC = nargs+1 | extra_nargs */
	uint64_t nargs1 = vm_raw_rc(ins) + vmf->multres1;
	return vm_bc_call_regular_handler(HANDLER_ARGUMENTS, nargs1);
}

static void *uj_BC_CALL(HANDLER_SIGNATURE)
{
	/* RA = base, (RB = nresults+1,) RC = nargs+1 | extra_nargs */
	uint64_t nargs1 = vm_raw_rc(ins);
	return vm_bc_call_regular_handler(HANDLER_ARGUMENTS, nargs1);
}

static LJ_AINLINE void *vm_bc_call_tail_handler(HANDLER_SIGNATURE,
						uint16_t nargs1)
{
	TValue *slot_ra = vm_slot_ra(base, ins);
	GCfunc *fn;

	if (LJ_UNLIKELY(!tvisfunc(slot_ra)))
		nargs1 = vm_vmeta_call(HANDLER_ARGUMENTS, slot_ra, nargs1);

	/* BC_CALLT_Z */
	fn = funcV(slot_ra);
	vm_set_vmstate(vmf->L,
		       UJ_VMST_INTERP); /* INTERP until a new BASE is setup */
	pc = restore_PC(base);

	if (LJ_UNLIKELY((uintptr_t)pc & FRAME_TYPE)) {
		/* Tailcall from vararg function */
		uintptr_t pc_varg_diff = (uintptr_t)pc - FRAME_VARG;

		if (LJ_UNLIKELY(!(pc_varg_diff & FRAME_TYPEP))) {
			/* Vararg frame below? Need to relocate BASE down */
			base = (TValue *)((char *)base - pc_varg_diff);
			pc = restore_PC(base);
		}
	}

	vmf->multres1 = nargs1;
	vm_copy_slots(base, slot_ra + 1, nargs1 - 1);

	if (LJ_UNLIKELY(isffunc(fn))) {
		/* Tailcall to a fast function */
		if (((uintptr_t)pc & FRAME_TYPE) == FRAME_LUA) {
			TValue *prev_base =
				restore_base(base, (base - 1)->fr.tp.pcr);
			vmf->kbase = setup_kbase(prev_base);
		}
	}

	(base - 1)->gcr = (GCobj *)fn;
	pc = fn->l.pc;

	/* NYI: uj_iprof_tick */
	return DISPATCH_CALL(nargs1);
}

static void *uj_BC_CALLMT(HANDLER_SIGNATURE)
{
	/* RA = base, RD = extra_nargs */
	uint64_t nargs1 = vm_raw_rd(ins) + vmf->multres1;
	return vm_bc_call_tail_handler(HANDLER_ARGUMENTS, nargs1);
}

static void *uj_BC_CALLT(HANDLER_SIGNATURE)
{
	/* RA = base, RD = nargs+1 */
	uint64_t nargs1 = vm_raw_rd(ins);
	return vm_bc_call_tail_handler(HANDLER_ARGUMENTS, nargs1);
}

static void *uj_BC_ITERC(HANDLER_SIGNATURE)
{
	/* RA = base, (RB = nresults+1,) RC = nargs+1 (2+1) */
	TValue *slot_ra = vm_slot_ra(base, ins) + 1;

	*slot_ra = *(slot_ra - 3); /* Copy state. fb[0] = fb[-3]. */
	*(slot_ra + 1) = *(slot_ra - 2); /* Copy control var. fb[1] = fb[-2]. */
	*(slot_ra - 1) = *(slot_ra - 4); /* Copy callable. fb[-1] = fb[-4]. */

	/*
	 * Originally this bytecode does not set INTERP vmstate for some reason
	 * (unlike all BC_CALL* bytecodes). Now it will set it that seems to be
	 * right (but maybe can break something).
	 */
	return vm_bc_call_regular_handler(HANDLER_ARGUMENTS, 2 + 1);
}

static void *uj_BC_ITERN(HANDLER_SIGNATURE)
{
	/* RA = base, (RB = nresults+1, RC = nargs+1 (2+1)) */
	TValue *slot_ra = vm_slot_ra(base, ins);
	GCtab *t = tabV(slot_ra - 2); /* table we're iterating on */
	/* Current iteration index (control_var->u32.hi == LJ_ITERN_MARK) */
	TValue *control_var = slot_ra - 1;

	/* Lower 4 bytes contain the actual value */
	size_t idx = control_var->u32.lo;
	while (inarray(t, idx)) {
		/* Traverse array part */
		TValue *arr_value = arrayslot(t, idx);

		if (tvisnil(arr_value)) { /* Skip holes in array part */
			++idx;
			continue;
		}

		/* Copy array slot to returned value */
		*(slot_ra + 1) = *arr_value;
		/* Return array index as a numeric key. */
		setnumV(slot_ra, idx);

		/* Update control var */
		++idx;
		control_var->u32.lo = idx;

		return DISPATCH();
	}

	idx -= t->asize;
	while (idx <= t->hmask) {
		/* Traverse hash part */
		Node *n = &t->node[idx];

		if (tvisnil(&n->val)) { /* Skip holes in hash part */
			++idx;
			continue;
		}

		/* Copy key and value from hash slot. */
		*slot_ra = n->key;
		*(slot_ra + 1) = n->val;

		/* Update control var (we previously decremented it by t->asize) */
		idx += t->asize + 1;
		control_var->u32.lo = idx;

		return DISPATCH();
	}

	/* End of iteration */
	setnilV(slot_ra);
	return DISPATCH();
}

static void *uj_BC_VARG(HANDLER_SIGNATURE)
{
	/* RA = base, RB = nresults+1, RC = numparams */
	TValue *slot_ra = vm_slot_ra(base, ins);
	/* base_varg may now be even _above_ BASE if nargs was < numparams */
	// clang-format off
	TValue *base_varg = (TValue *)((char *)base + sizeof(TValue) + FRAME_VARG - (char *)frame_ftsz(base - 1))
			    + vm_raw_rc(ins);
	// clang-format on
	TValue *end = slot_ra + vm_raw_rb(ins) - 1;
	TValue *dst;
	uint8_t nres1 = vm_raw_rb(ins);
	uint8_t nvargs1;

	if (nres1 > 0) {
		if (base_varg < base) {
			do {
				*slot_ra = *(base_varg - 1);
				slot_ra++;
				base_varg++;

				if (slot_ra >= end)
					return DISPATCH();
			} while (base_varg < base);
		}

		do {
			setnilV(slot_ra);
			slot_ra++;
		} while (slot_ra < end);

		return DISPATCH();
	}

	/* Copy all varargs */
	vmf->multres1 = 1;

	if (base < base_varg)
		return DISPATCH();

	nvargs1 = base - base_varg;
	vmf->multres1 = nvargs1 + 1;

	dst = slot_ra + nvargs1;

	if (dst > vmf->L->maxstack) {
		save_PC(pc);

		vmf->L->base = base;
		vmf->L->top = slot_ra;

		uj_state_stack_grow(vmf->L, vmf->multres1 - 1);

		base = vmf->L->base;
		slot_ra = vmf->L->top;
		// clang-format off
		base_varg = (TValue *)((char *)base + sizeof(TValue) + FRAME_VARG - (char *)frame_ftsz(base - 1))
			    + vm_raw_rc(ins);
		// clang-format on
	}

	do {
		*slot_ra = *(base_varg - 1);
		slot_ra++;
		base_varg++;
	} while (base_varg < base);

	return DISPATCH();
}

static void *uj_BC_ISNEXT(HANDLER_SIGNATURE)
{
	/* RA = base, RD = target (points to ITERN) */
	TValue *slot_ra = vm_slot_ra(base, ins);
	TValue *ctrl_var = slot_ra - 1;
	BCIns *prev_pc;
	ptrdiff_t offset;

	if (LJ_UNLIKELY(!tvisfunc(slot_ra - 3)))
		/* callable is not a function */
		goto despecialize;

	if (LJ_UNLIKELY(!tvistab(slot_ra - 2)))
		/* state is not a table */
		goto despecialize;

	if (LJ_UNLIKELY(!tvisnil(ctrl_var)))
		/* control variable is not nil */
		goto despecialize;

	if (LJ_UNLIKELY(funcV(slot_ra - 3)->c.ffid != FF_next_N))
		/* callable is not next FF */
		goto despecialize;

	pc += vm_rj(ins); /* jump to ITERN */

	/* Initialize control var to 0, set upper 4 bytes to LJ_ITERN_MARK. */
	settag(ctrl_var, LJ_TNUMX);
	ctrl_var->u32.hi = LJ_ITERN_MARK;
	ctrl_var->u32.lo = 0;

	return DISPATCH();

despecialize:
	/* Despecialize bytecode if any of the checks fail. */
	setbc_op(pc - 1, BC_JMP); /* replace ISNEXT with JMP */
	prev_pc = pc;

	pc += vm_rj(ins); /* jump to ITERN */

	setbc_op(pc, BC_ITERC); /* replace ITERN with ITERC */
	setbc_op(pc + 1, BC_ITERL); /* replace ITRNL with ITERL */

	/* Original comment from DynASM source:
	 * BC_ITERL target should be also restored, because
	 * it may be erased by BC_JITRNL trace number.
	 * target_pos = iterl_pos + 1 + offset
	 * offset = target_pos - iterl_pos - 1 = PC/4 - (PC_2 + 4)/4 - 1
	 * offset = (PC - PC_2) >> 2 - 2
	 */

	/* We don't have to divide by 4 since sizeof(BCIns) is 4 */
	offset = (prev_pc - pc) - 2;
	/* Implicitly adds BCBIAS_J to offset */
	setbc_j(pc + 1, offset);

	return DISPATCH();
}

static LJ_AINLINE void *vm_bc_ret_handler(HANDLER_SIGNATURE,
					  uint64_t added_multres)
{
	/*
	 * ra_offset is used to fix RA when returning from vararg function.
	 * Only for RET1, RET and RETM. Unused in RET0.
	 */
	uintptr_t ra_offset =
		vm_raw_ra(ins) * (sizeof(TValue) / 2); /* x2 encoding */
	/*
	 * added_multres is the only difference between RET and RETM. In case of RETM
	 * RD may be 0, but added_multres is guaranteed to be >= 1. See comment in
	 * original RETM in dasc.
	 */
	vmf->multres1 = vm_raw_rd(ins) + added_multres;
	setbc_d_raw(&ins, vmf->multres1);

	/* INTERP until the old BASE is restored */
	vm_set_vmstate(vmf->L, UJ_VMST_INTERP);

restore_PC:
	pc = restore_PC(base);

	if ((uintptr_t)pc & FRAME_TYPE) {
		uintptr_t pc_varg_diff = (uintptr_t)pc - FRAME_VARG;

		/* Not returning to a fixarg Lua func? */
		if (pc_varg_diff & FRAME_TYPEP)
			return vm_return(HANDLER_ARGUMENTS, ra_offset);

		/* Return from vararg function: relocate BASE down and RA up */
		base = (TValue *)((char *)base - pc_varg_diff);
		ra_offset += pc_varg_diff;
		goto restore_PC;
	}

	/* NB! Setting single result to nil is needed only for RET0 since
	 * vm_nil_slots() below doesn't handle case when expected_nres1==1
	 * and vmf->multres1==1 (so probably not needed at all but it's
	 * done in original interpreter). For RET1, RET and RETM this nil
	 * has no effect and will be overwritten shortly below.
	 */
	setnilV(base - 1);

	TValue *results = (TValue *)((ptrdiff_t)base + ra_offset);
	return bc_ret_z(HANDLER_ARGUMENTS, results);
}

static void *uj_BC_RETM(HANDLER_SIGNATURE)
{
	/* RA = results, RD = nresults+1 */
	return vm_bc_ret_handler(HANDLER_ARGUMENTS, vmf->multres1);
}

static void *uj_BC_RET(HANDLER_SIGNATURE)
{
	/* RA = results, RD = nresults+1 */
	return vm_bc_ret_handler(HANDLER_ARGUMENTS, 0ull);
}

static void *uj_BC_RET0(HANDLER_SIGNATURE)
{
	/* RA = results, RD = nresults+1 */
	return vm_bc_ret_handler(HANDLER_ARGUMENTS, 0ull);
}

static void *uj_BC_RET1(HANDLER_SIGNATURE)
{
	/* RA = results, RD = nresults+1 */
	return vm_bc_ret_handler(HANDLER_ARGUMENTS, 0ull);
}

static void *uj_BC_HOTCNT(HANDLER_SIGNATURE)
{
	/* NYI: Payload */
	return DISPATCH();
}

static void *uj_BC_COVERG(HANDLER_SIGNATURE)
{
#ifdef UJIT_COVERAGE
	lua_State *L = vmf->L;

	L->base = base;
	uj_coverage_stream_line(L, pc);
	base = L->base;
#endif
	return DISPATCH();
}

static void *uj_BC_FORI(HANDLER_SIGNATURE)
{
	UJ_PEDANTIC_OFF

	static void *comparator[] = {&&positive_step, &&non_positive_step};
	TValue *idx;
	lua_Number i, stop;

	vm_check_timeout(vmf, base, pc);

	idx = vm_slot_ra(base, ins);

	if (LJ_UNLIKELY(!tvisnum(idx) || !tvisnum(idx + 1) ||
			!tvisnum(idx + 2))) {
		save_PC(pc);

		vmf->L->base = base;
		uj_meta_for(vmf->L, idx);
		/* Base sync looks redundant, but original interpreter does it */
		base = vmf->L->base;
		idx = vm_slot_ra(base, ins); /* Recompute if base changed */
	}

	i = numV(idx);
	setnumV(idx + 3, i);
	stop = numV(idx + 1);

	goto *comparator[rawV(idx + 2) >> 63];

positive_step:
	if (stop < i)
		pc += vm_rj(ins);
	return DISPATCH();

non_positive_step:
	if (stop > i)
		pc += vm_rj(ins);
	return DISPATCH();

	UJ_PEDANTIC_ON
}

static void *uj_BC_IFORL(HANDLER_SIGNATURE)
{
	UJ_PEDANTIC_OFF

	static void *comparator[] = {&&positive_step, &&non_positive_step};
	TValue *idx;
	lua_Number i, stop;

	vm_check_timeout(vmf, base, pc);

	idx = vm_slot_ra(base, ins);

#ifndef NDEBUG
	if (LJ_UNLIKELY(!tvisnum(idx + 1) || !tvisnum(idx + 2)))
		abort();
#endif

	i = numV(idx);
	stop = numV(idx + 1);

	i += numV(idx + 2);
	setnumV(idx, i);
	setnumV(idx + 3, i);

	goto *comparator[rawV(idx + 2) >> 63];

positive_step:
	if (stop >= i)
		pc += vm_rj(ins);
	return DISPATCH();

non_positive_step:
	if (stop <= i)
		pc += vm_rj(ins);
	return DISPATCH();

	UJ_PEDANTIC_ON
}

static void *uj_BC_IITERL(HANDLER_SIGNATURE)
{
	/* RA = base, RD = target */
	TValue *slot_ra = vm_slot_ra(base, ins);

	vm_check_timeout(vmf, base, pc);

	if (!tvisnil(slot_ra)) {
		pc += vm_rj(ins);
		*(slot_ra - 1) = *slot_ra;
	}

	return DISPATCH();
}

static void *uj_BC_IITRNL(HANDLER_SIGNATURE)
{
	/* RA = base, RD = target */
	TValue *slot_ra = vm_slot_ra(base, ins);

	vm_check_timeout(vmf, base, pc);

	if (!tvisnil(slot_ra))
		pc += vm_rj(ins);

	return DISPATCH();
}

static void *uj_BC_ILOOP(HANDLER_SIGNATURE)
{
	vm_check_timeout(vmf, base, pc);
	return DISPATCH();
}

static void *uj_BC_JMP(HANDLER_SIGNATURE)
{
	pc += vm_rj(ins);
	return DISPATCH();
}

static void *uj_BC_IFUNCF(HANDLER_SIGNATURE)
{
	/* BASE = new base, RA = framesize, RD = nargs+1 */
	TValue *top = vm_slot_ra(base, ins);
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);
	const GCproto *pt = pc2proto(pc - 1);

	vmf->kbase = setup_kbase(base);
	vm_set_atomic_vmstate(L, base, UJ_VMST_LFUNC);

	if (top > L->maxstack) {
		uint8_t framesize = pt->framesize;

		vmf->pc = pc + 1; /* Must point after first instruction */

		L->base = base;
		L->top = base + nargs1 - 1;
		uj_state_stack_grow(L, framesize);
		base = L->base;
	}

	vm_check_timeout(vmf, base, pc); /* Must be after stack check */

	/* Clear missing parameters */
	for (ptrdiff_t i = vm_raw_rd(ins) - 1; i < pt->numparams; i++)
		setnilV(base + i);

	return DISPATCH();
}

static void *uj_BC_IFUNCV(HANDLER_SIGNATURE)
{
	/* BASE = new base, RA = framesize, RD = nargs+1 */
	lua_State *L = vmf->L;
	uint8_t nargs1 = vm_raw_rd(ins); /* Number of actual args */
	TValue *base_new = base + nargs1;
	const GCproto *pt = pc2proto(pc - 1);
	uint8_t numparams = pt->numparams; /* Number of fixed args (can be 0) */
	uint64_t ftsz = nargs1 * sizeof(TValue) + FRAME_VARG;
	TValue *base_tmp;

	(base_new - 1)->u64 = (base - 1)->u64; /* Store copy of LFUNC */
	(base_new - 1)->u64_hi = ftsz; /* Store delta + FRAME_VARG */

	vm_set_atomic_vmstate(L, base, UJ_VMST_LFUNC);

	if (base_new + pt->framesize > L->maxstack) {
		vmf->pc = pc + 1; /* Must point after first instruction */
		L->base = base;
		L->top = base + nargs1 - 1;

		uj_state_stack_grow(L, pt->framesize);

		base = L->base;
		base_new = base + nargs1; /* Update if stack reallocated */
	}

	vm_check_timeout(vmf, base, pc); /* Must be after stack check */

	base_tmp = base_new;
	while (numparams != 0) {
		base++;

		if (base >= base_new) {
			setnilV(base_tmp);
			base_tmp++;
		} else {
			*base_tmp = *(base - 1);
			base_tmp++;
			setnilV(base - 1);
		}

		numparams--;
	}

	base = base_new;
	vmf->kbase = setup_kbase(base);

	return DISPATCH();
}

static void *uj_BC_FUNCC(HANDLER_SIGNATURE)
{
	/* BASE = new base, RA = ins RA|RD (unused), RD = nargs+1 */
	lua_State *L = vmf->L;
	int got_nres;
	uint32_t restored_RA;

	L->base = base;
	L->top = base + vm_raw_rd(ins) - 1;

	if (L->top + LUA_MINSTACK > L->maxstack) {
		uj_state_stack_grow(L, LUA_MINSTACK);
		base = L->base;
	}

	vm_check_timeout(vmf, base, pc); /* Must be after stack check */

	vm_set_atomic_vmstate(L, base, UJ_VMST_CFUNC);
	got_nres = base2func(L->base).c.f(L); /* (lua_State *L) */
	/* INTERP until jump to BC_RET* or vm_return */
	vm_set_vmstate(L, UJ_VMST_INTERP);

	base = L->base;
	pc = restore_PC(base); /* Fetch PC of caller */

	/*
	 * Restores RA of previous instruction (since there is no access to
	 * it from FUNCC) to be used in vm_return.
	 */
	restored_RA = (L->top - (base + got_nres)) * sizeof(TValue);
	return vm_returnc(HANDLER_ARGUMENTS, restored_RA, got_nres);
}

/* This bytecode is almost the same as BC_FUNCC, except that C function called through a wrapper */
static void *uj_BC_FUNCCW(HANDLER_SIGNATURE)
{
	/* BASE = new base, RA = ins RA|RD (unused), RD = nargs+1 */
	lua_State *L = vmf->L;
	int got_nres;
	lua_CFunction f;
	int (*wrapper)(lua_State *L, lua_CFunction f);
	uint32_t restored_RA;

	L->base = base;
	L->top = base + vm_raw_rd(ins) - 1;

	if (L->top + LUA_MINSTACK > L->maxstack) {
		uj_state_stack_grow(L, LUA_MINSTACK);
		base = L->base;
	}

	vm_check_timeout(vmf, base, pc); /* Must be after stack check */

	f = base2func(L->base).c.f;
	UJ_PEDANTIC_OFF
	wrapper = (void *)G(L)->wrapf;
	UJ_PEDANTIC_ON

	vm_set_atomic_vmstate(L, base, UJ_VMST_CFUNC);
	got_nres = wrapper(L, f);
	/* INTERP until jump to BC_RET* or vm_return */
	vm_set_vmstate(L, UJ_VMST_INTERP);

	base = L->base;
	pc = restore_PC(base); /* Fetch PC of caller */

	/* See comment in FUNCC */
	restored_RA = (L->top - (base + got_nres)) * sizeof(TValue);
	return vm_returnc(HANDLER_ARGUMENTS, restored_RA, got_nres);
}

static void *uj_ff_assert(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);
	TValue *tmp;

	vm_set_vmstate_ffunc(L, base);

	if (LJ_UNLIKELY(nargs1 < FFUNC_NARGS(1)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	if (!tvistruecond(base))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	pc = restore_PC(base);
	vmf->multres1 = nargs1;

	*(base - 1) = *base;

	if (nargs1 == FFUNC_NARGS(1))
		return vm_fff_res_(HANDLER_ARGUMENTS);

	/* Guaranteed to be > 2 */
	nargs1 -= 2;

	tmp = base;

	do {
		*(tmp) = *(tmp + 1);
		tmp++;
		nargs1--;
	} while (nargs1 != 0);

	return vm_fff_res_(HANDLER_ARGUMENTS);
}

static void *uj_ff_type(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);

	vm_set_vmstate_ffunc(L, base);

	if (LJ_UNLIKELY(nargs1 < FFUNC_NARGS(1)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	pc = restore_PC(base);

	TValue *tv = &funcV(base - 1)->c.upvalue[~gettag(base)];
	*(base - 1) = *tv;

	return vm_fff_res1(HANDLER_ARGUMENTS);
}

static void *uj_ff_next(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);
	int has_more_items;

	vm_set_vmstate_ffunc(L, base);

	if (LJ_UNLIKELY(nargs1 < FFUNC_NARGS(1)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	if (LJ_UNLIKELY(nargs1 == FFUNC_NARGS(1)))
		/* Set missing 2nd arg to nil */
		setnilV(base + 1);

	if (!tvistab(base))
		return vm_fff_fallback(HANDLER_ARGUMENTS, FFUNC_NARGS(2));

	pc = restore_PC(base);

	save_PC(pc);

	L->base = base;
	L->top = base;
	has_more_items = lj_tab_next(L, tabV(base), base + 1);
	base = L->base;

	if (!has_more_items) {
		/* End of traversal: return nil. */
		setnilV(base - 1);
		return vm_fff_res1(HANDLER_ARGUMENTS);
	}

	/* Copy key and value to results. */
	*(base - 1) = *(base + 1);
	*base = *(base + 2);

	return vm_fff_res(HANDLER_ARGUMENTS, FFUNC_NARGS(2));
}

static void *uj_ff_pairs(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);

	vm_set_vmstate_ffunc(L, base);

	if (LJ_UNLIKELY(nargs1 < FFUNC_NARGS(1)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	if (LJ_UNLIKELY(!tvistab(base)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, FFUNC_NARGS(1));

#if LJ_52
	if (LJ_UNLIKELY(tabV(base)->metatable != NULL))
		return vm_fff_fallback(HANDLER_ARGUMENTS, FFUNC_NARGS(1));
#endif

	pc = restore_PC(base);

	/* ff_next will be contained as an upvalue in base - 1 */
	TValue *callable = &funcV(base - 1)->c.upvalue[0];
	*(base - 1) = *callable;

	setnilV(base + 1);

	return vm_fff_res(HANDLER_ARGUMENTS, FFUNC_NARGS(3));
}

static void *uj_ff_ipairs_aux(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);
	int32_t idx;
	GCtab *t;
	const TValue *v;

	vm_set_vmstate_ffunc(L, base);

	if (LJ_UNLIKELY(nargs1 < FFUNC_NARGS(1)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	if (LJ_UNLIKELY(!tvistab(base)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, FFUNC_NARGS(2));

	if (LJ_UNLIKELY(!tvisnum(base + 1)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, FFUNC_NARGS(2));

	pc = restore_PC(base);

	idx = (int32_t)(numV(base + 1) + 1.0);
	setnumV(base - 1, (double)idx);

	t = tabV(base);
	if (inarray(t, idx)) {
		TValue *arr_value = arrayslot(t, idx);

		if (tvisnil(arr_value)) /* Encountered a hole */
			return vm_fff_res0(HANDLER_ARGUMENTS);

		*base = *arr_value;
		return vm_fff_res2(HANDLER_ARGUMENTS);
	}

	if (LJ_LIKELY(t->hmask == 0))
		return vm_fff_res0(HANDLER_ARGUMENTS);

	v = lj_tab_getinth(t, idx);
	if (v == NULL || tvisnil(v))
		return vm_fff_res0(HANDLER_ARGUMENTS);

	*base = *v;
	return vm_fff_res2(HANDLER_ARGUMENTS);
}

static void *uj_ff_ipairs(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);
	TValue *callable;

	vm_set_vmstate_ffunc(L, base);

	if (LJ_UNLIKELY(nargs1 < FFUNC_NARGS(1)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	if (LJ_UNLIKELY(!tvistab(base)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, FFUNC_NARGS(1));

#if LJ_52
	if (LJ_UNLIKELY(tabV(base)->metatable != NULL))
		return vm_fff_fallback(HANDLER_ARGUMENTS, FFUNC_NARGS(1));
#endif

	pc = restore_PC(base);

	/* ipairs_aux will be contained as an upvalue in base - 1 */
	callable = &funcV(base - 1)->c.upvalue[0];
	*(base - 1) = *callable;

	setnumV(base + 1, 0.0);

	return vm_fff_res(HANDLER_ARGUMENTS, FFUNC_NARGS(3));
}

static void *uj_ff_getmetatable(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);
	GCtab *mt;
	const TValue *meta;

	vm_set_vmstate_ffunc(L, base);

	if (LJ_UNLIKELY(nargs1 < FFUNC_NARGS(1)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	pc = restore_PC(base);

	if (LJ_LIKELY(tvistab(base)))
		mt = tabV(base)->metatable;
	else if (tvisudata(base))
		mt = udataV(base)->metatable;
	else
		mt = uj_mtab_get_for_otype(G(L), base);

	if (mt == NULL) {
		setnilV(base - 1);
		return vm_fff_res1(HANDLER_ARGUMENTS);
	}

	meta = lj_tab_getstr(mt, uj_meta_name(G(L), MM_metatable));

	if (!meta || tvisnil(meta)) {
		settabV(L, base - 1, mt);
		return vm_fff_res1(HANDLER_ARGUMENTS);
	}

	*(base - 1) = *meta;
	return vm_fff_res1(HANDLER_ARGUMENTS);
}

static void *uj_ff_setmetatable(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);
	GCtab *t;

	vm_set_vmstate_ffunc(L, base);

	if (LJ_UNLIKELY(nargs1 < FFUNC_NARGS(2)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	if (LJ_UNLIKELY(!tvistab(base)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, FFUNC_NARGS(2));

	t = tabV(base);

	if (LJ_UNLIKELY(uj_obj_is_immutable(obj2gco(t))))
		return vm_fff_fallback(HANDLER_ARGUMENTS, FFUNC_NARGS(2));

	if (LJ_UNLIKELY(t->metatable != NULL))
		return vm_fff_fallback(HANDLER_ARGUMENTS, FFUNC_NARGS(2));

	if (LJ_UNLIKELY(!tvistab(base + 1)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, FFUNC_NARGS(2));

	t->metatable = tabV(base + 1);
	pc = restore_PC(base);
	*(base - 1) = *base; /* Return original table */

	lj_gc_anybarriert(L, t);

	return vm_fff_res1(HANDLER_ARGUMENTS);
}

static void *uj_ff_rawget(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);

	vm_set_vmstate_ffunc(L, base);

	if (LJ_UNLIKELY(nargs1 < FFUNC_NARGS(2)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	if (LJ_UNLIKELY(!tvistab(base)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, vm_raw_rd(ins));

	pc = restore_PC(base);
	*(base - 1) = *lj_tab_get(L, tabV(base), base + 1);
	return vm_fff_res1(HANDLER_ARGUMENTS);
}

static void *uj_ff_tonumber(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);

	vm_set_vmstate_ffunc(L, base);

	if (LJ_UNLIKELY(nargs1 != FFUNC_NARGS(1)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	/* Only handles the number case inline (without a base argument) */

	if (tvisnum(base)) {
		double num = numV(base);
		pc = restore_PC(base);
		setnumV(base - 1, num);
		return vm_fff_res1(HANDLER_ARGUMENTS);
	}

	return vm_fff_fallback(HANDLER_ARGUMENTS, FFUNC_NARGS(1));
}

static void *uj_ff_tostring(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);

	vm_set_vmstate_ffunc(L, base);

	if (LJ_UNLIKELY(nargs1 < FFUNC_NARGS(1)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	pc = restore_PC(base);

	/* Only handles the string or number case inline */

	if (tvisstr(base)) {
		/* A __tostring method in the string base metatable is ignored */
		*(base - 1) = *base;
		return vm_fff_res1(HANDLER_ARGUMENTS);
	}

	if (tvisnum(base) && L->glref->gcroot[GCROOT_BASEMT_NUM] == NULL) {
		GCstr *str;

		save_PC(pc);
		/* Handle numbers inline, unless a number base metatable is present */
		L->base = base;
		base = vm_ffgccheck(vmf, pc, FFUNC_NARGS(1));
		str = uj_str_fromnumber(L, numV(base));
		base = L->base;
		setstrV(L, base - 1, str);
		return vm_fff_res1(HANDLER_ARGUMENTS);
	}

	return vm_fff_fallback(HANDLER_ARGUMENTS, FFUNC_NARGS(1));
}

static void *uj_ff_pcall(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);
	uint8_t hookmask;
	uint64_t ftsz;

	vm_set_vmstate_ffunc(L, base);

	if (LJ_UNLIKELY(nargs1 < FFUNC_NARGS(1)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	hookmask = (G(L)->hookmask >> HOOK_ACTIVE_SHIFT) & 1;
	ftsz = sizeof(TValue) + FRAME_PCALL + hookmask;

	nargs1--; /* -1 to remove function arg from callee arguments list */
	base++;
	pc = (BCIns *)(uintptr_t)ftsz;

	return vm_call_dispatch(HANDLER_ARGUMENTS, base, nargs1);
}

static void *uj_ff_xpcall(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);
	TValue tmptv;
	uint8_t hookmask;
	uint64_t ftsz;

	vm_set_vmstate_ffunc(L, base);

	if (LJ_UNLIKELY(nargs1 < FFUNC_NARGS(2)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	if (LJ_UNLIKELY(!tvisfunc(base + 1)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	tmptv = *(base + 1);
	*(base + 1) = *base;
	*base = tmptv;

	hookmask = (G(L)->hookmask >> HOOK_ACTIVE_SHIFT) & 1;
	ftsz = sizeof(TValue) * 2 + FRAME_PCALL + hookmask;

	nargs1 -= 2; /* -2 to remove function itself and callback */
	base += 2;
	pc = (BCIns *)(uintptr_t)ftsz;

	return vm_call_dispatch(HANDLER_ARGUMENTS, base, nargs1);
}

static void *uj_ff_coroutine_yield(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);

	vm_set_vmstate_ffunc(L, base);

	vm_check_timeout(vmf, base, pc);

	if (((uintptr_t)L->cframe & CFRAME_RESUME) == 0)
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	L->base = base;
	L->top = base + nargs1 - 1;
	L->cframe = NULL;
	L->status = LUA_YIELD;

	/* vm_leave_unw */
	vm_set_vmstate(L, UJ_VMST_INTERP);
	G(L)->top_frame.guesttop.interp_base = L->base;
	vm_restore_vmstate(L, vmf);

	vm_check_cinterp_magic_num(vmf);
	return (void *)(ptrdiff_t)LUA_YIELD;
}

static void vm_coroutine_setup_vm_frame(lua_State *L, struct vm_frame *vmf)
{
	vm_save_vmstate(L, vmf);

	/* SAVE_L */
	vmf->L = L;
	/* SAVE_PC */
	vmf->pc = NULL;
	vmf->cframe_prev = NULL;
	/* SAVE_NRES */
	vmf->nres1 = 0ul;
	/* SAVE_ERRF */
	vmf->errf = 0ul;
#if __x86_64__
	/* Save original return address */
	vmf->interp_ret_addr = *(uint64_t *)((ptrdiff_t)vmf - 8);
#endif
	vmf->c_interp_marker = C_INTERPRETER_MAGIC_NUM;

	L->cframe = (void *)((uintptr_t)vmf | CFRAME_RESUME);
}

void *uj_coroutine_bc_ret_z(lua_State *L, BCIns *pc, TValue *base,
			    void __attribute__((unused)) * unused1,
			    void __attribute__((unused)) * unused2,
			    struct vm_frame *vmf)
{
	BCIns ins = *pc;
	uint16_t nargs1 = (uint16_t)(L->top - base) + 1; /* ptrdiff2nargs */

	vm_coroutine_setup_vm_frame(L, vmf);
	vm_set_vmstate(L, UJ_VMST_INTERP);
	vmf->multres1 = nargs1;
	return bc_ret_z(pc, base, vmf, ins, base);
}

void *uj_coroutine_vm_return(lua_State *L, BCIns *pc, TValue *base, BCIns ins,
			     ptrdiff_t ra_offset, struct vm_frame *vmf)
{
	uint16_t nargs1 = (uint16_t)(L->top - base) + 1; /* ptrdiff2nargs */

	vm_coroutine_setup_vm_frame(L, vmf);
	vm_set_vmstate(L, UJ_VMST_INTERP);
	vmf->multres1 = nargs1;
	return vm_return(pc, base, vmf, ins, ra_offset);
}

void *uj_initial_coroutine_call(lua_State *L, TValue *base, int frame_type,
				void __attribute__((unused)) * unused1,
				void __attribute__((unused)) * unused2,
				struct vm_frame *vmf)
{
	vm_coroutine_setup_vm_frame(L, vmf);
	vm_set_vmstate(L, UJ_VMST_INTERP);
	return vm_execute_first_bytecode(vmf, base, frame_type);
}

int uj_vm_resume(lua_State *L, TValue *base)
{
	uint16_t nargs1 = (uint16_t)(L->top - base) + 1; /* ptrdiff2nargs */
	int status;
	BCIns *pc;

	if (L->status == 0)
		/* Initial resume (like a call) */
		return (ptrdiff_t)uj_prepare_initial_coroutine_call(L, base,
								    FRAME_CP);

	/* Resume after yield (like a return) */
	L->status = 0;

	base = L->base;
	pc = restore_PC(base);

	if (((uintptr_t)pc & FRAME_TYPE) == 0) {
		status = (ptrdiff_t)uj_prepare_coroutine_bc_ret_z(L, pc, base);
	} else {
		ptrdiff_t ra_offset = (base - L->base) * sizeof(TValue);
		BCIns ins = 0;

		setbc_d_raw(&ins, nargs1);
		status = (ptrdiff_t)uj_prepare_coroutine_vm_return(
			L, pc, base, ins, ra_offset);
	}

	return status;
}

static void *uj_ff_coroutine_resume(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);
	lua_State *co;
	TValue *coro_top;
	TValue *p;
	int ret;

	vm_set_vmstate_ffunc(L, base);

	if (LJ_UNLIKELY(nargs1 < FFUNC_NARGS(1)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	pc = restore_PC(base);
	save_PC(pc);

	co = (lua_State *)base->gcr;

	if (LJ_UNLIKELY(!tvisthread(base)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	if (uj_state_has_timeout(co))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	if (co->cframe != NULL)
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	if (co->status > LUA_YIELD)
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	/* Status != LUA_YIELD (i.e. 0)? Check for presence of initial func */
	if (co->status != LUA_YIELD && co->base == co->top)
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	coro_top = co->top;
	p = coro_top + nargs1 - 2;

	if (p > co->maxstack)
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	co->top = p;
	L = vmf->L;
	L->base = base;
	base++; /* Keep resumed thread in stack for GC */
	L->top = base;

	for (uint16_t i = 0; i != nargs1; ++i) {
		*(coro_top + i) = *(base + i);
	}

	ret = uj_vm_resume(co, coro_top);
	/* Resumed coroutine returned, INTERP until jump to BC_RET* or vm_return */
	vm_set_vmstate(L, UJ_VMST_INTERP);
	base = L->base;

	if (ret > LUA_YIELD) {
		/* Coroutine returned with error (at co->top-1) */
		setboolV(base - 1, 0);
		co->top--; /* Clear error from coroutine stack */
		*base = *co->top; /* Copy error message */
		setbc_d_raw(&ins, 1 + 2); /* nresults+1 = 1 + false + error */
		vmf->multres1 = 1 + 2;
	} else {
		int nres;

		coro_top = co->top;
		co->top = co->base;
		nres = coro_top - co->base;

		if (nres == 0) {
			/* No results? */
			setbc_d_raw(&ins, 1 + 1); /* 1 + true */
			vmf->multres1 = 1 + 1;
		} else {
			if (base + nres > L->maxstack) {
				uj_state_stack_grow(L, nres);
				base = L->base;
			}

			vmf->multres1 = nres + 2; /* 1 + true + results */
			setbc_d_raw(&ins, vmf->multres1);

			do {
				*(base + nres - 1) = *(co->base + nres - 1);
				nres--;
			} while (nres != 0);
		}

		setboolV(base - 1, 1);
	}

	pc = vmf->pc;

	if (((uintptr_t)pc & FRAME_TYPE) == 0)
		return bc_ret_z(HANDLER_ARGUMENTS, base - 1);

	return vm_return(HANDLER_ARGUMENTS, -sizeof(TValue));
}

/* coroutine.wrap is very close to coroutine.resume */
static void *uj_ff_coroutine_wrap_aux(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);
	lua_State *co;
	TValue *coro_top, *p;
	int ret;

	vm_set_vmstate_ffunc(L, base);

	pc = restore_PC(base);
	save_PC(pc);

	co = (lua_State *)funcV(base - 1)->c.upvalue[0].gcr;

	if (uj_state_has_timeout(co))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	if (co->cframe != NULL)
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	if (co->status > LUA_YIELD)
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	/* Status != LUA_YIELD (i.e. 0)? Check for presence of initial func */
	if (co->status != LUA_YIELD && co->base == co->top)
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	coro_top = co->top;
	p = coro_top + nargs1 - 1;

	if (p > co->maxstack)
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	co->top = p;
	L = vmf->L;
	L->base = base;
	base++; /* Keep resumed thread in stack for GC */
	L->top = base;

	for (uint16_t i = 0; i != nargs1; ++i)
		*(coro_top + i) = *(base + i - 1);

	ret = uj_vm_resume(co, coro_top);
	/* Resumed coroutine returned, INTERP until jump to BC_RET* or vm_return */
	vm_set_vmstate(L, UJ_VMST_INTERP);
	base = L->base;

	if (ret > LUA_YIELD) {
		lj_ffh_coroutine_wrap_err(L, co);
		/* Unreachable */
	} else {
		int nres;

		coro_top = co->top;
		co->top = co->base;
		nres = coro_top - co->base;

		if (nres == 0) {
			/* No results? */
			setbc_d_raw(&ins, 1);
			vmf->multres1 = 1;
		} else {
			if (base + nres > L->maxstack) {
				uj_state_stack_grow(L, nres);
				base = L->base;
			}

			vmf->multres1 = nres + 1; /* 1 + results */
			setbc_d_raw(&ins, vmf->multres1);

			do {
				*(base + nres - 1) = *(co->base + nres - 1);
				nres--;
			} while (nres != 0);
		}
	}

	pc = vmf->pc;

	if (((uintptr_t)pc & FRAME_TYPE) == 0)
		return bc_ret_z(HANDLER_ARGUMENTS, base);

	return vm_return(HANDLER_ARGUMENTS, 0ll);
}

#define math_ff_prologue_1_arg                                     \
	lua_State *L = vmf->L;                                     \
	uint16_t nargs1 = vm_raw_rd(ins);                          \
                                                                   \
	vm_set_vmstate_ffunc(L, base);                             \
                                                                   \
	if (LJ_UNLIKELY(nargs1 < FFUNC_NARGS(1)))                  \
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1); \
                                                                   \
	if (!tvisnum(base))                                        \
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1); \
                                                                   \
	pc = restore_PC(base);

#define math_builtin_1_arg(func_name)                          \
	static void *uj_ff_math_##func_name(HANDLER_SIGNATURE) \
	{                                                      \
		math_ff_prologue_1_arg;                        \
                                                               \
		setnumV(base - 1, func_name(numV(base)));      \
		return vm_fff_res1(HANDLER_ARGUMENTS);         \
	}

#define math_builtin_2_args(func_name)                                     \
	static void *uj_ff_math_##func_name(HANDLER_SIGNATURE)             \
	{                                                                  \
		lua_State *L = vmf->L;                                     \
		uint16_t nargs1 = vm_raw_rd(ins);                          \
                                                                           \
		vm_set_vmstate_ffunc(L, base);                             \
                                                                           \
		if (LJ_UNLIKELY(nargs1 < FFUNC_NARGS(2)))                  \
			return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1); \
                                                                           \
		if (!tvisnum(base) || !tvisnum(base + 1))                  \
			return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1); \
                                                                           \
		pc = restore_PC(base);                                     \
                                                                           \
		setnumV(base - 1, func_name(numV(base), numV(base + 1)));  \
		return vm_fff_res1(HANDLER_ARGUMENTS);                     \
	}

#define math_minmax(name, func_name)                                       \
	static void *uj_ff_math_##name(HANDLER_SIGNATURE)                  \
	{                                                                  \
		lua_State *L = vmf->L;                                     \
		uint16_t nargs1 = vm_raw_rd(ins);                          \
		double result;                                             \
                                                                           \
		vm_set_vmstate_ffunc(L, base);                             \
                                                                           \
		if (!tvisnum(base))                                        \
			return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1); \
                                                                           \
		pc = restore_PC(base);                                     \
                                                                           \
		result = numV(base);                                       \
                                                                           \
		while (nargs1 > FFUNC_NARGS(1)) {                          \
			TValue *val = base + nargs1 - 2;                   \
                                                                           \
			if (!tvisnum(val))                                 \
				return vm_fff_fallback(HANDLER_ARGUMENTS,  \
						       nargs1);            \
                                                                           \
			result = func_name(numV(val), result);             \
			nargs1--;                                          \
		}                                                          \
                                                                           \
		setnumV(base - 1, result);                                 \
		return vm_fff_res1(HANDLER_ARGUMENTS);                     \
	}

UJ_PEDANTIC_OFF
math_builtin_1_arg(fabs);
math_builtin_1_arg(floor);
math_builtin_1_arg(ceil);
math_builtin_1_arg(sqrt);
math_builtin_1_arg(log10);
math_builtin_1_arg(exp);
math_builtin_1_arg(sin);
math_builtin_1_arg(cos);
math_builtin_1_arg(tan);
math_builtin_1_arg(asin);
math_builtin_1_arg(acos);
math_builtin_1_arg(atan);
math_builtin_1_arg(sinh);
math_builtin_1_arg(cosh);
math_builtin_1_arg(tanh);

math_builtin_2_args(atan2);
math_builtin_2_args(pow);
math_builtin_2_args(fmod);
math_builtin_2_args(ldexp);

math_minmax(min, fmin);
math_minmax(max, fmax);
UJ_PEDANTIC_ON

static void *uj_ff_math_log(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);

	vm_set_vmstate_ffunc(L, base);

	/* Accepts exactly 1 arg, see fallback function */
	if (LJ_UNLIKELY(nargs1 != FFUNC_NARGS(1)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	if (!tvisnum(base))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	pc = restore_PC(base);

	setnumV(base - 1, log(numV(base)));
	return vm_fff_res1(HANDLER_ARGUMENTS);
}

static void *uj_ff_math_frexp(HANDLER_SIGNATURE)
{
	math_ff_prologue_1_arg;

	int exp;
	double m = frexp(numV(base), &exp);

	setnumV(base - 1, m);
	setnumV(base, exp);
	return vm_fff_res2(HANDLER_ARGUMENTS);
}

static void *uj_ff_math_modf(HANDLER_SIGNATURE)
{
	math_ff_prologue_1_arg;

	double integral_part;
	double fractional_part = modf(numV(base), &integral_part);

	setnumV(base - 1, integral_part);
	setnumV(base, fractional_part);
	return vm_fff_res2(HANDLER_ARGUMENTS);
}

/* Used in both math.rad and math.deg since the only difference is upvalue */
static void *uj_ff_math_rad_deg(HANDLER_SIGNATURE)
{
	math_ff_prologue_1_arg;

	double val = funcV(base - 1)->c.upvalue[0].n;

	setnumV(base - 1, val * numV(base));
	return vm_fff_res1(HANDLER_ARGUMENTS);
}

static LJ_AINLINE uint32_t normalize_bit_num(double num)
{
	static const uint64_t TOBIT_CONST =
		0x4338000000000000; /* 2^52 + 2^51 */
	double tobit_const;
	uint32_t ret;

	memcpy(&tobit_const, &TOBIT_CONST, sizeof(double));
	num += tobit_const;
	memcpy(&ret, &num, sizeof(uint32_t));

	return ret;
}

static void *uj_ff_bit_tobit(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);
	int32_t res;

	vm_set_vmstate_ffunc(L, base);

	if (LJ_UNLIKELY(nargs1 < FFUNC_NARGS(1)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	if (LJ_UNLIKELY(!tvisnum(base)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	res = normalize_bit_num(numV(base));

	pc = restore_PC(base);
	setnumV(base - 1, res);
	return vm_fff_res1(HANDLER_ARGUMENTS);
}

static void *uj_ff_bit_bnot(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);
	int32_t res;

	vm_set_vmstate_ffunc(L, base);

	if (LJ_UNLIKELY(nargs1 < FFUNC_NARGS(1)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	if (LJ_UNLIKELY(!tvisnum(base)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	res = ~normalize_bit_num(numV(base));

	pc = restore_PC(base);
	setnumV(base - 1, res);
	return vm_fff_res1(HANDLER_ARGUMENTS);
}

static void *uj_ff_bit_bswap(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);
	int32_t res;

	vm_set_vmstate_ffunc(L, base);

	if (LJ_UNLIKELY(nargs1 < FFUNC_NARGS(1)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	if (LJ_UNLIKELY(!tvisnum(base)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	res = __builtin_bswap32(normalize_bit_num(numV(base)));

	pc = restore_PC(base);
	setnumV(base - 1, res);
	return vm_fff_res1(HANDLER_ARGUMENTS);
}

#define ffunc_bit_op(name, op)                                             \
	static void *uj_ff_bit_##name(HANDLER_SIGNATURE)                   \
	{                                                                  \
		lua_State *L = vmf->L;                                     \
		uint16_t nargs1 = vm_raw_rd(ins);                          \
		int32_t res;                                               \
                                                                           \
		vm_set_vmstate_ffunc(L, base);                             \
                                                                           \
		if (LJ_UNLIKELY(nargs1 < FFUNC_NARGS(1)))                  \
			return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1); \
                                                                           \
		if (LJ_UNLIKELY(!tvisnum(base)))                           \
			return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1); \
                                                                           \
		res = normalize_bit_num(numV(base));                       \
                                                                           \
		for (int i = 1; i < nargs1 - 1; ++i) {                     \
			if (LJ_UNLIKELY(!tvisnum(base + i)))               \
				return vm_fff_fallback(HANDLER_ARGUMENTS,  \
						       nargs1);            \
			res = res op normalize_bit_num(numV(base + i));    \
		}                                                          \
                                                                           \
		pc = restore_PC(base);                                     \
		setnumV(base - 1, res);                                    \
		return vm_fff_res1(HANDLER_ARGUMENTS);                     \
	}

UJ_PEDANTIC_OFF
ffunc_bit_op(band, &);
ffunc_bit_op(bor, |);
ffunc_bit_op(bxor, ^);
UJ_PEDANTIC_ON

/*
 * Shifting by an amount greater than or equal to the number of bits in the
 * left operand is undefined behavior in C. This mask ensures the shift
 * amount stays within the valid range [0, 31], which is safe for 32-bit
 * integers that are used below.
*/
static uint32_t LJ_AINLINE truncate_shift_amount(uint32_t shift)
{
	static const uint32_t MAX_SHIFT_MASK = 31;
	return shift & MAX_SHIFT_MASK;
}

#define SAFE_SHIFT32(value, op, shift) (value) op truncate_shift_amount(shift)

static uint32_t LJ_AINLINE uj_vm_bit_op_shl(uint32_t value, uint32_t shift)
{
	return SAFE_SHIFT32(value, <<, shift);
}

static uint32_t LJ_AINLINE uj_vm_bit_op_shr(uint32_t value, uint32_t shift)
{
	return SAFE_SHIFT32(value, >>, shift);
}

static uint32_t LJ_AINLINE uj_vm_bit_op_sar(uint32_t value, uint32_t shift)
{
	return SAFE_SHIFT32((int32_t)value, >>, shift);
}

static uint32_t LJ_AINLINE uj_vm_bit_op_rol(uint32_t value, uint32_t shift)
{
	shift = truncate_shift_amount(shift);
	return (value << shift) | (SAFE_SHIFT32(value, >>, 32 - shift));
}

static uint32_t LJ_AINLINE uj_vm_bit_op_ror(uint32_t value, uint32_t shift)
{
	shift = truncate_shift_amount(shift);
	return (value >> shift) | (SAFE_SHIFT32(value, <<, 32 - shift));
}

#define ffunc_bit_sh(name, op)                                             \
	static void *uj_ff_bit_##name(HANDLER_SIGNATURE)                   \
	{                                                                  \
		lua_State *L = vmf->L;                                     \
		uint16_t nargs1 = vm_raw_rd(ins);                          \
		uint32_t a, b;                                             \
		int32_t res;                                               \
                                                                           \
		vm_set_vmstate_ffunc(L, base);                             \
                                                                           \
		if (LJ_UNLIKELY(nargs1 < FFUNC_NARGS(2)))                  \
			return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1); \
                                                                           \
		if (LJ_UNLIKELY(!tvisnum(base)))                           \
			return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1); \
                                                                           \
		if (LJ_UNLIKELY(!tvisnum(base + 1)))                       \
			return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1); \
                                                                           \
		a = normalize_bit_num(numV(base));                         \
		b = normalize_bit_num(numV(base + 1));                     \
		res = uj_vm_bit_op_##op(a, b);                             \
                                                                           \
		pc = restore_PC(base);                                     \
		setnumV(base - 1, res);                                    \
                                                                           \
		return vm_fff_res1(HANDLER_ARGUMENTS);                     \
	}

UJ_PEDANTIC_OFF
ffunc_bit_sh(lshift, shl);
ffunc_bit_sh(rshift, shr);
ffunc_bit_sh(arshift, sar);
ffunc_bit_sh(rol, rol);
ffunc_bit_sh(ror, ror);
UJ_PEDANTIC_ON

static void *uj_ff_string_len(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);
	double len;

	vm_set_vmstate_ffunc(L, base);

	if (LJ_UNLIKELY(nargs1 < FFUNC_NARGS(1)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	if (LJ_UNLIKELY(!tvisstr(base)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	len = (double)strV(base)->len;
	pc = restore_PC(base);
	setnumV(base - 1, len);
	return vm_fff_res1(HANDLER_ARGUMENTS);
}

static void *uj_ff_string_byte(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);
	GCstr *s;

	vm_set_vmstate_ffunc(L, base);

	if (LJ_UNLIKELY(nargs1 != FFUNC_NARGS(1)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	if (LJ_UNLIKELY(!tvisstr(base)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	s = strV(base);
	pc = restore_PC(base);
	if (s->len == 0) /* Return no results for empty string */
		return vm_fff_res0(HANDLER_ARGUMENTS);

	setnumV(base - 1, (double)(unsigned char)strdata(s)[0]);
	return vm_fff_res1(HANDLER_ARGUMENTS);
}

static void *vm_ff_newstr(HANDLER_SIGNATURE, const char *str, size_t len)
{
	lua_State *L = vmf->L;
	GCstr *s;

	save_PC(pc);

	L->base = base;
	s = uj_str_new(L, str, len);
	base = L->base;

	pc = restore_PC(base);
	setstrV(L, base - 1, s);
	return vm_fff_res1(HANDLER_ARGUMENTS);
}

/*
 * Special vm_ff_newstr version for uj_ff_string_char to help
 * compiler insert tail call instead of actual call.
 */
static BCIns *vm_ff_newstr_char(HANDLER_SIGNATURE, char c, size_t len)
{
	lua_State *L = vmf->L;
	GCstr *s;

	save_PC(pc);

	L->base = base;
	s = uj_str_new(L, &c, len);
	base = L->base;

	pc = restore_PC(base);
	setstrV(L, base - 1, s);
	return pc;
}

static LJ_AINLINE void *vm_ff_emptystr(HANDLER_SIGNATURE)
{
	return vm_ff_newstr(HANDLER_ARGUMENTS, "", 0);
}

static void *uj_ff_string_char(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);
	int num;
	char c;

	vm_set_vmstate_ffunc(L, base);

	L->base = base;
	base = vm_ffgccheck(vmf, pc, nargs1);

	if (LJ_UNLIKELY(nargs1 != FFUNC_NARGS(1)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	if (LJ_UNLIKELY(!tvisnum(base)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	num = (int)numV(base);
	if (LJ_UNLIKELY(num > 255u)) /* unsigned compare */
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	c = (char)num;
	pc = vm_ff_newstr_char(HANDLER_ARGUMENTS, c, 1);
	base = L->base;
	return vm_fff_res1(HANDLER_ARGUMENTS);
}

static void *uj_ff_string_sub(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);
	int end;
	TValue *stv, *starttv;
	GCstr *s;
	int start;

	vm_set_vmstate_ffunc(L, base);

	L->base = base;
	base = vm_ffgccheck(vmf, pc, nargs1);

	if (nargs1 < FFUNC_NARGS(2))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	end = -1;
	if (nargs1 > FFUNC_NARGS(2)) { /* end argument is set */
		TValue *endtv = base + 2;

		if (LJ_UNLIKELY(!tvisnum(endtv)))
			return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

		end = (int)numV(endtv);
	}

	stv = base;
	if (LJ_UNLIKELY(!tvisstr(stv)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	starttv = base + 1;
	if (LJ_UNLIKELY(!tvisnum(starttv)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	s = strV(stv);
	start = (int)numV(starttv);

	if (s->len < (size_t)end) {
		/* Negative end or overflow (unsigned compare) */

		if (end > 0)
			end = s->len;
		else /* Overflow */
			end = end + s->len + 1;
	}

	/* Negative start */
	if (start < 0)
		start = start + s->len + 1;

	if (start <= 0) /* Underflow */
		start = 1;

	if (start > end) /* Range underflow */
		return vm_ff_emptystr(HANDLER_ARGUMENTS);

	return vm_ff_newstr(HANDLER_ARGUMENTS, strdata(s) + start - 1,
			    end - start + 1);
}

static void *uj_ff_string_rep(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);
	int count;
	GCstr *str;
	char *bufstr;

	vm_set_vmstate_ffunc(L, base);

	L->base = base;
	base = vm_ffgccheck(vmf, pc, nargs1);

	if (nargs1 != FFUNC_NARGS(2))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	if (LJ_UNLIKELY(!tvisstr(base)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	if (LJ_UNLIKELY(!tvisnum(base + 1)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	count = (int)numV(base + 1);
	if (count <= 0)
		return vm_ff_emptystr(HANDLER_ARGUMENTS);

	str = strV(base);
	if (str->len == 0)
		return vm_ff_emptystr(HANDLER_ARGUMENTS);

	/* Only handle the 1-char case inline. */
	if (str->len > 1)
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	/* Note: not using public API here, so sb->sz won't be valid */
	bufstr = uj_sbuf_tmp_bytes(L, count);
	memset(bufstr, strdata(str)[0], count);

	return vm_ff_newstr(HANDLER_ARGUMENTS, bufstr, count);
}

static void *uj_ff_string_reverse(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);
	GCstr *str;
	char *bufstr;

	vm_set_vmstate_ffunc(L, base);

	L->base = base;
	base = vm_ffgccheck(vmf, pc, nargs1);

	if (LJ_UNLIKELY(nargs1 < FFUNC_NARGS(1)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	if (LJ_UNLIKELY(!tvisstr(base)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	str = strV(base);
	if (str->len == 0)
		return vm_ff_emptystr(HANDLER_ARGUMENTS);

	/* Note: not using public API here, so sb->sz won't be valid */
	bufstr = uj_sbuf_tmp_bytes(L, str->len);
	for (size_t i = 0; i < str->len; ++i)
		bufstr[str->len - i - 1] = strdata(str)[i];

	return vm_ff_newstr(HANDLER_ARGUMENTS, bufstr, str->len);
}

static void *vm_ffstring_case(HANDLER_SIGNATURE, char lo, char hi)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);
	GCstr *str;
	char *bufstr;

	vm_set_vmstate_ffunc(L, base);

	L->base = base;
	base = vm_ffgccheck(vmf, pc, nargs1);

	if (LJ_UNLIKELY(nargs1 < FFUNC_NARGS(1)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	if (LJ_UNLIKELY(!tvisstr(base)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	str = strV(base);
	if (str->len == 0)
		return vm_ff_emptystr(HANDLER_ARGUMENTS);

	/* Note: not using public API here, so sb->sz won't be valid */
	bufstr = uj_sbuf_tmp_bytes(L, str->len);
	for (size_t i = 0; i < str->len; ++i) {
		char c = strdata(str)[i];

		if (c >= lo && c <= hi)
			c ^= 0x20;
		bufstr[i] = c;
	}

	return vm_ff_newstr(HANDLER_ARGUMENTS, bufstr, str->len);
}

static void *uj_ff_string_lower(HANDLER_SIGNATURE)
{
	return vm_ffstring_case(HANDLER_ARGUMENTS, 0x41, 0x5a);
}

static void *uj_ff_string_upper(HANDLER_SIGNATURE)
{
	return vm_ffstring_case(HANDLER_ARGUMENTS, 0x61, 0x7a);
}

static void *uj_ff_table_getn(HANDLER_SIGNATURE)
{
	lua_State *L = vmf->L;
	uint16_t nargs1 = vm_raw_rd(ins);

	vm_set_vmstate_ffunc(L, base);

	if (LJ_UNLIKELY(nargs1 < FFUNC_NARGS(1)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	if (LJ_UNLIKELY(!tvistab(base)))
		return vm_fff_fallback(HANDLER_ARGUMENTS, nargs1);

	pc = restore_PC(base);
	setnumV(base - 1, lj_tab_len(tabV(base)));
	return vm_fff_res1(HANDLER_ARGUMENTS);
}
