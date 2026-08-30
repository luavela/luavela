/*
 * Instruction dispatch handling.
 * Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "lj_obj.h"
#include "uj_dispatch.h"
#include "uj_ff.h"
#if LJ_HASJIT
#include "jit/lj_trace.h"
#endif /* LJ_HASJIT */
#include "uj_hotcnt.h"

/* Bump GG_NUM_ASMFF in uj_dispatch.h as needed. Ugly. */
LJ_STATIC_ASSERT(GG_NUM_ASMFF == FF_NUM_ASMFUNC);

/* -- Dispatch table management ------------------------------------------- */

#ifndef UJIT_CINTERP
static LJ_AINLINE ASMFunction dispatch_bc_semantic(uint32_t op)
{
	return lj_bc_ptr[op];
}

static LJ_AINLINE void dispatch_as(ASMFunction *disp, uint32_t op,
				   ASMFunction f)
{
	disp[op] = f;
}

static LJ_AINLINE void dispatch_as_default(ASMFunction *disp, uint32_t op)
{
	disp[op] = dispatch_bc_semantic(op);
}
#endif

static LJ_AINLINE CInterpFunction dispatch_bc_semantic_cinterp(uint32_t op)
{
	return uj_vm_bc_dispatch_cinterp[op];
}

static LJ_AINLINE void dispatch_as_cinterp(CInterpFunction *disp, uint32_t op,
					   CInterpFunction f)
{
	disp[op] = f;
}

static LJ_AINLINE void dispatch_as_default_cinterp(CInterpFunction *disp,
						   uint32_t op)
{
	disp[op] = dispatch_bc_semantic_cinterp(op);
}

/* Initialize instruction dispatch table and hot counters. */
void uj_dispatch_init(struct GG_State *GG)
{
	uint32_t op;
#ifndef UJIT_CINTERP
	ASMFunction *disp = GG->dispatch;
#endif
	CInterpFunction *disp_cinterp = GG->dispatch_cinterp;

	/* Setup main dispatch table (bytecodes and ff pseudo-bytecodes). */
	for (op = 0; op < GG_LEN_DDISP; op++) {
#ifndef UJIT_CINTERP
		dispatch_as_default(disp, op);
#endif
		dispatch_as_default_cinterp(disp_cinterp, op);
	}

	/*
	 * Setup auxilary (static) dispatch table
	 * for 'default' bytecode semantics.
	 */
	for (op = 0; op < GG_LEN_SDISP; op++) {
#ifndef UJIT_CINTERP
		dispatch_as(disp, GG_LEN_DDISP + op, dispatch_bc_semantic(op));
#endif
		dispatch_as_cinterp(disp_cinterp, GG_LEN_DDISP + op,
				    dispatch_bc_semantic_cinterp(op));
	}

	/*
	 * Since JIT part is disabled by default, treat profiling
	 * instructions as interpreted system-wide.
	 * This is overwritten later when JIT is turned on.
	 */
#ifndef UJIT_CINTERP
	dispatch_as(disp, BC_FORL, dispatch_bc_semantic(BC_IFORL));
	dispatch_as(disp, BC_ITERL, dispatch_bc_semantic(BC_IITERL));
	dispatch_as(disp, BC_ITRNL, dispatch_bc_semantic(BC_IITRNL));
	dispatch_as(disp, BC_LOOP, dispatch_bc_semantic(BC_ILOOP));
	dispatch_as(disp, BC_FUNCF, dispatch_bc_semantic(BC_IFUNCF));
	dispatch_as(disp, BC_FUNCV, dispatch_bc_semantic(BC_IFUNCV));
#endif

	dispatch_as_cinterp(disp_cinterp, BC_FORL,
			    dispatch_bc_semantic_cinterp(BC_IFORL));
	dispatch_as_cinterp(disp_cinterp, BC_ITERL,
			    dispatch_bc_semantic_cinterp(BC_IITERL));
	dispatch_as_cinterp(disp_cinterp, BC_ITRNL,
			    dispatch_bc_semantic_cinterp(BC_IITRNL));
	dispatch_as_cinterp(disp_cinterp, BC_LOOP,
			    dispatch_bc_semantic_cinterp(BC_ILOOP));
	dispatch_as_cinterp(disp_cinterp, BC_FUNCF,
			    dispatch_bc_semantic_cinterp(BC_IFUNCF));
	dispatch_as_cinterp(disp_cinterp, BC_FUNCV,
			    dispatch_bc_semantic_cinterp(BC_IFUNCV));

	GG->g.bc_cfunc_int = BCINS_AD(BC_FUNCC, LUA_MINSTACK, 0);
	GG->g.bc_cfunc_ext = GG->g.bc_cfunc_int;
	for (op = 0; op < GG_NUM_ASMFF; op++)
		GG->bcff[op] = BCINS_AD(BC__MAX + op, 0, 0);
}

#if LJ_HASJIT
/* Initialize hotcount opcodes. */
void uj_dispatch_init_hotcount(struct global_State *g)
{
	uj_hotcnt_patch_protos(g);
}
#endif /* LJ_HASJIT */

/* Internal dispatch mode bits. */
#define DISPMODE_JIT 0x01 /* JIT compiler on. */
#define DISPMODE_REC 0x02 /* Recording active. */
#define DISPMODE_INS 0x04 /* Override instruction dispatch. */
#define DISPMODE_CALL 0x08 /* Override call dispatch. */
#define DISPMODE_RET 0x10 /* Override return dispatch. */

/* Helper function to set dynamic return dispatch. */
static void dispatch_set_dynamic_return(struct global_State *g, uint8_t mode)
{
#ifndef UJIT_CINTERP
	ASMFunction *disp = G2GG(g)->dispatch;
#endif
	CInterpFunction *disp_cinterp = G2GG(g)->dispatch_cinterp;

	if (mode & DISPMODE_RET) {
#ifndef UJIT_CINTERP
		dispatch_as(disp, BC_RETM, lj_vm_rethook);
		dispatch_as(disp, BC_RET, lj_vm_rethook);
		dispatch_as(disp, BC_RET0, lj_vm_rethook);
		dispatch_as(disp, BC_RET1, lj_vm_rethook);
#endif

		dispatch_as_cinterp(disp_cinterp, BC_RETM, uj_vm_rethook);
		dispatch_as_cinterp(disp_cinterp, BC_RET, uj_vm_rethook);
		dispatch_as_cinterp(disp_cinterp, BC_RET0, uj_vm_rethook);
		dispatch_as_cinterp(disp_cinterp, BC_RET1, uj_vm_rethook);
	} else {
#ifndef UJIT_CINTERP
		dispatch_as_default(disp, BC_RETM);
		dispatch_as_default(disp, BC_RET);
		dispatch_as_default(disp, BC_RET0);
		dispatch_as_default(disp, BC_RET1);
#endif

		dispatch_as_default_cinterp(disp_cinterp, BC_RETM);
		dispatch_as_default_cinterp(disp_cinterp, BC_RET);
		dispatch_as_default_cinterp(disp_cinterp, BC_RET0);
		dispatch_as_default_cinterp(disp_cinterp, BC_RET1);
	}
}

/* Helper function to set dynamic call dispatch */
static void dispatch_set_dynamic_call(struct global_State *g, uint8_t mode)
{
#ifndef UJIT_CINTERP
	ASMFunction *disp = G2GG(g)->dispatch;
#endif
	CInterpFunction *disp_cinterp = G2GG(g)->dispatch_cinterp;

	uint32_t op;
	for (op = GG_LEN_SDISP; op < GG_LEN_DDISP; op++) {
		if (mode & DISPMODE_CALL) {
#ifndef UJIT_CINTERP
			dispatch_as(disp, op, lj_vm_callhook);
#endif
			dispatch_as_cinterp(disp_cinterp, op, uj_vm_callhook);
		} else { /* No call hooks? */
#ifndef UJIT_CINTERP
			dispatch_as_default(disp, op);
#endif
			dispatch_as_default_cinterp(disp_cinterp, op);
		}
	}
}

/* Helper function to set instruction hooks or recording dispatch.*/
static void dispatch_set_record_or_hooks(struct global_State *g, uint8_t mode)
{
#ifndef UJIT_CINTERP
	ASMFunction *disp = G2GG(g)->dispatch;
#endif
	CInterpFunction *disp_cinterp = G2GG(g)->dispatch_cinterp;

	BCOp op;
	/* The recording dispatch also checks for hooks. */
#ifndef UJIT_CINTERP
	ASMFunction f = mode & DISPMODE_REC ? lj_vm_record : lj_vm_inshook;
#endif
	for (op = 0; op < GG_LEN_SDISP; op++) {
		if (op == BC_HOTCNT)
			continue;

#ifndef UJIT_CINTERP
		dispatch_as(disp, op, f);
#endif
		dispatch_as_cinterp(disp_cinterp, op, uj_vm_inshook);
	}
}

/* Helper function that sets dispatch table for given mode */
static void dispatch_update(struct global_State *g, uint8_t mode)
{
	uint8_t oldmode = g->dispatchmode;
	const uint8_t DISPMODE_REC_OR_INS = DISPMODE_REC | DISPMODE_INS;
	ASMFunction *disp = G2GG(g)->dispatch;
	CInterpFunction *disp_cinterp = G2GG(g)->dispatch_cinterp;
#ifndef UJIT_CINTERP
	ASMFunction f_forl, f_iterl, f_itrnl, f_loop, f_funcf, f_funcv;
#endif
	CInterpFunction f_forl_cinterp, f_iterl_cinterp, f_itrnl_cinterp,
		f_loop_cinterp, f_funcf_cinterp, f_funcv_cinterp;
	g->dispatchmode = mode;

#ifndef UJIT_CINTERP
	/* Hotcount if JIT is on, but not while recording. */
	if ((mode & (DISPMODE_JIT | DISPMODE_REC)) == DISPMODE_JIT) {
		f_forl = dispatch_bc_semantic(BC_FORL);
		f_iterl = dispatch_bc_semantic(BC_ITERL);
		f_itrnl = dispatch_bc_semantic(BC_ITRNL);
		f_loop = dispatch_bc_semantic(BC_LOOP);
		f_funcf = dispatch_bc_semantic(BC_FUNCF);
		f_funcv = dispatch_bc_semantic(BC_FUNCV);
	} else {
		/* Otherwise use the non-hotcounting instructions. */
		f_forl = dispatch_bc_semantic(BC_IFORL);
		f_iterl = dispatch_bc_semantic(BC_IITERL);
		f_itrnl = dispatch_bc_semantic(BC_IITRNL);
		f_loop = dispatch_bc_semantic(BC_ILOOP);
		f_funcf = dispatch_bc_semantic(BC_IFUNCF);
		f_funcv = dispatch_bc_semantic(BC_IFUNCV);
	}
	/*
	 * Init static counting instruction dispatch first
	 * (may be copied below).
	 */
	dispatch_as(disp, GG_LEN_DDISP + BC_FORL, f_forl);
	dispatch_as(disp, GG_LEN_DDISP + BC_ITERL, f_iterl);
	dispatch_as(disp, GG_LEN_DDISP + BC_ITRNL, f_itrnl);
	dispatch_as(disp, GG_LEN_DDISP + BC_LOOP, f_loop);
#endif

	f_forl_cinterp = dispatch_bc_semantic_cinterp(BC_IFORL);
	f_iterl_cinterp = dispatch_bc_semantic_cinterp(BC_IITERL);
	f_itrnl_cinterp = dispatch_bc_semantic_cinterp(BC_IITRNL);
	f_loop_cinterp = dispatch_bc_semantic_cinterp(BC_ILOOP);
	f_funcf_cinterp = dispatch_bc_semantic_cinterp(BC_IFUNCF);
	f_funcv_cinterp = dispatch_bc_semantic_cinterp(BC_IFUNCV);

	dispatch_as_cinterp(disp_cinterp, GG_LEN_DDISP + BC_FORL,
			    f_forl_cinterp);
	dispatch_as_cinterp(disp_cinterp, GG_LEN_DDISP + BC_ITERL,
			    f_iterl_cinterp);
	dispatch_as_cinterp(disp_cinterp, GG_LEN_DDISP + BC_ITRNL,
			    f_itrnl_cinterp);
	dispatch_as_cinterp(disp_cinterp, GG_LEN_DDISP + BC_LOOP,
			    f_loop_cinterp);

	/* Set dynamic instruction dispatch. */
	if ((oldmode ^ mode) & DISPMODE_REC_OR_INS) {
		/* Need to update the whole table. */
		if (!(mode & DISPMODE_REC_OR_INS)) {
			/* No ins dispatch? */
			/* Copy static dispatch table to dynamic one. */
			memcpy(&disp[0], &disp[GG_LEN_DDISP],
			       GG_LEN_SDISP * sizeof(ASMFunction));
			memcpy(&disp_cinterp[0], &disp_cinterp[GG_LEN_DDISP],
			       GG_LEN_SDISP * sizeof(CInterpFunction));
			/* Overwrite with dynamic return dispatch. */
			dispatch_set_dynamic_return(g, mode);
		} else {
			dispatch_set_record_or_hooks(g, mode);
		}
	} else if (!(mode & DISPMODE_REC_OR_INS)) {
		/* Otherwise set dynamic counting ins. */
#ifndef UJIT_CINTERP
		dispatch_as(disp, BC_FORL, f_forl);
		dispatch_as(disp, BC_ITERL, f_iterl);
		dispatch_as(disp, BC_ITRNL, f_itrnl);
		dispatch_as(disp, BC_LOOP, f_loop);
#endif

		dispatch_as_cinterp(disp_cinterp, BC_FORL, f_forl_cinterp);
		dispatch_as_cinterp(disp_cinterp, BC_ITERL, f_iterl_cinterp);
		dispatch_as_cinterp(disp_cinterp, BC_ITRNL, f_itrnl_cinterp);
		dispatch_as_cinterp(disp_cinterp, BC_LOOP, f_loop_cinterp);

		dispatch_set_dynamic_return(g, mode);
	}

	/* Set dynamic call dispatch. */
	if ((oldmode ^ mode) & DISPMODE_CALL) /* Update the whole table? */
		dispatch_set_dynamic_call(g, mode);

	if (!(mode & DISPMODE_CALL)) { /* Overwrite dynamic counting ins. */
#ifndef UJIT_CINTERP
		dispatch_as(disp, BC_FUNCF, f_funcf);
		dispatch_as(disp, BC_FUNCV, f_funcv);
#endif

		dispatch_as_cinterp(disp_cinterp, BC_FUNCF, f_funcf_cinterp);
		dispatch_as_cinterp(disp_cinterp, BC_FUNCV, f_funcv_cinterp);
	}

#if LJ_HASJIT
	/* Reset hotcounts for JIT off to on transition. */
	if ((mode & DISPMODE_JIT) && !(oldmode & DISPMODE_JIT))
		uj_dispatch_init_hotcount(g);
#endif /* LJ_HASJIT */
}

/* Update dispatch table depending on various flags. */
void uj_dispatch_update(struct global_State *g)
{
	uint8_t mode = 0;
#if LJ_HASJIT
	mode |= G2J(g)->flags & JIT_F_ON ? DISPMODE_JIT : 0;
	if (G2J(g)->state != LJ_TRACE_IDLE)
		mode |= DISPMODE_REC | DISPMODE_INS | DISPMODE_CALL;
#endif /* LJ_HASJIT */
	mode |= g->hookmask & (LUA_MASKLINE | LUA_MASKCOUNT) ? DISPMODE_INS : 0;
	mode |= g->hookmask & LUA_MASKCALL ? DISPMODE_CALL : 0;
	mode |= g->hookmask & LUA_MASKRET ? DISPMODE_RET : 0;

	if (g->dispatchmode != mode) /* Mode changed? */
		dispatch_update(g, mode);
}
