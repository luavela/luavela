/*
 * Interfaces for managing the virtual machine host stack frame.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJ_CFRAME_H
#define _UJ_CFRAME_H

#include "lj_bcins.h"

/*
 * Marshalled layout of the virtual machine host stack frame.
 * The frame is crafted inside the VM assembler code, so this
 * description must match its dynasm counterpart (see vm_x86.dasc).
 */
struct cframe {
	uint32_t tmp1;
	uint32_t tmp2;
	uint64_t tmpa;
	uint64_t tmpa2;
	TValue tmptv;
	int32_t vmstate;
	uint32_t multres;
	int64_t nres;
	int64_t errf;
	struct lua_State *L;
	BCIns *pc;
	void *cframe_prev;
	uint64_t savereg_r14;
	uint64_t savereg_r15;
	uint64_t savereg_rbx;
	uint64_t savereg_rbp;
	uint64_t savereg_rip;
};

/* $rsp is always 16-byte aligned as per x64 ABI p.3.2.2 */
LJ_STATIC_ASSERT(sizeof(struct cframe) % 16 == 0);

static LJ_AINLINE int64_t uj_cframe_errfunc(const void *rsp)
{
	return ((const struct cframe *)rsp)->errf;
}

static LJ_AINLINE void uj_cframe_errfunc_inherit(void *rsp)
{
	((struct cframe *)rsp)->errf = -1;
}

static LJ_AINLINE int64_t uj_cframe_nres(const void *rsp)
{
	return ((const struct cframe *)rsp)->nres;
}

static LJ_AINLINE void uj_cframe_nres_set(void *rsp, int64_t nresults)
{
	((struct cframe *)rsp)->nres = nresults;
}

static LJ_AINLINE void *uj_cframe_prev(const void *rsp)
{
	return ((const struct cframe *)rsp)->cframe_prev;
}

#if LJ_HASFFI
static LJ_AINLINE void uj_cframe_prev_set(void *rsp, const void *prev)
{
	((struct cframe *)rsp)->cframe_prev = (void *)prev;
}
#endif /* LJ_HASFFI */

static LJ_AINLINE uint32_t uj_cframe_multres(const void *rsp)
{
	return ((const struct cframe *)rsp)->multres;
}

static LJ_AINLINE BCIns *uj_cframe_pc(const void *rsp)
{
	return ((const struct cframe *)rsp)->pc;
}

static LJ_AINLINE void uj_cframe_pc_set(void *rsp, const BCIns *ins)
{
	((struct cframe *)rsp)->pc = (BCIns *)ins;
}

static LJ_AINLINE struct lua_State *uj_cframe_L(const void *rsp)
{
	return ((const struct cframe *)rsp)->L;
}

#if LJ_HASFFI
static LJ_AINLINE void uj_cframe_L_set(void *rsp, const struct lua_State *state)
{
	((struct cframe *)rsp)->L = (struct lua_State *)state;
}
#endif /* LJ_HASFFI */

#define CFRAME_SIZE_JIT (sizeof(struct cframe) + 32)

#define CFRAME_RESUME 1
#define CFRAME_UNWIND_FF 2 /* Only used in unwinder. */
#define CFRAME_RAWMASK (~(intptr_t)(CFRAME_RESUME | CFRAME_UNWIND_FF))

static LJ_AINLINE intptr_t uj_cframe_canyield(const void *cframe_ptr)
{
	return (intptr_t)cframe_ptr & CFRAME_RESUME;
}

static LJ_AINLINE void *uj_cframe_raw(const void *cframe_ptr)
{
	return (void *)((intptr_t)(cframe_ptr)&CFRAME_RAWMASK);
}

static LJ_AINLINE BCIns *uj_cframe_Lpc(const struct lua_State *L)
{
	return uj_cframe_pc(uj_cframe_raw(L->cframe));
}

/*
 * Only used in unwinder: If an exception is going to be caught by an (x)pcall
 * fast function, mark corresponding pointer to the host frame.
 */
static LJ_AINLINE void *uj_cframe_unwind_mark_ff(const void *cframe_ptr)
{
	return (void *)((intptr_t)cframe_ptr | CFRAME_UNWIND_FF);
}

/*
 * Only used in unwinder: Check if a pointer to the host frame was marked by
 * uj_cframe_unwind_mark_ff, i.e. if the host frame "holds" an (x)pcall guest
 * frame which will catch the excpetion.
 */
static LJ_AINLINE intptr_t uj_cframe_unwind_is_ff(const void *cframe_ptr)
{
	return (intptr_t)cframe_ptr & CFRAME_UNWIND_FF;
}

#endif /* !_UJ_CFRAME_H */
