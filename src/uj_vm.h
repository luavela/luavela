/*
 * Bytecode interpreter implemented in C.
 * Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJ_VM_H
#define _UJ_VM_H

#include "lj_vm.h"
#include "uj_cframe.h"

// clang-format off
#if !defined(__apple_build_version__)
#if __clang__
static_assert(__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__ >= 200102,
	"clang version should be at least 20.1.2");
#else
static_assert(__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__ >= 140200,
	"gcc version should be at least 14.2.0");
#endif
#endif
// clang-format on

#define C_INTERPRETER_MAGIC_NUM 0xBADC0FFEE0DDF00D

struct lua_State;

typedef TValue *(*lua_CPFunction)(lua_State *L, lua_CFunction func, void *ud);

/* Layout corresponds to cframe. */
struct vm_frame {
	uint32_t tmp1;
	uint32_t tmp2;
	uint64_t tmpa;
	uint64_t tmpa2;
	TValue tmptv;
	struct vmstate_context vmsc;
	uint32_t multres1;
	int64_t nres1;
	int64_t errf;
	struct lua_State *L;
	BCIns *pc;
	void *cframe_prev;
	uint64_t c_interp_marker; /* Overlaps with savereg_r14 in cframe */
#if __x86_64__
	uint64_t saved_gpr[6]; /* rbx, rbp, r12 - r15 */
	void *kbase;
	uint64_t interp_ret_addr;
#else
	uint64_t saved_gpr[11]; /* x19 - x29 */
	uint64_t _padding; /* Aligns saved_fpr by 16 bytes - probably redundant */
	uint8_t saved_fpr[8 * 16]; /* v8 - v15, 128 bits */
	void *kbase;
	/* Address where entry function should return (on interpreter exit) */
	uint64_t entry_func_return_addr;
	/* Address loaded to link register in landing pad after returning from
	 * exceptions handling. Points right after bl instruction in entry function */
	uint64_t vm_return_addr;
#endif
};

void uj_vm_call(lua_State *L, TValue *base, int nres1);
int uj_vm_pcall(lua_State *L, TValue *base, int nres1, ptrdiff_t ef);
int uj_vm_cpcall(lua_State *L, lua_CFunction func, void *ud, lua_CPFunction cp);

int uj_vm_resume(lua_State *L, TValue *base);

void uj_vm_ff_landing_pad(void);
void uj_vm_c_landing_pad(void);

void *uj_vm_prepare_call(lua_State *L, TValue *base, int nres1, ptrdiff_t ef,
			 int frame_type);
void *uj_vm_prepare_cpcall(lua_State *L, lua_CFunction func, void *ud,
			   lua_CPFunction cp);
void *uj_prepare_coroutine_bc_ret_z(lua_State *L, BCIns *pc, TValue *base);
void *uj_prepare_coroutine_vm_return(lua_State *L, BCIns *pc, TValue *base,
				     BCIns ins, ptrdiff_t ra_offset);
void *uj_prepare_initial_coroutine_call(lua_State *L, TValue *base,
					int frame_type);

void *uj_vm_inshook(BCIns *pc, TValue *base, struct vm_frame *vmf, BCIns ins);
void *uj_vm_rethook(BCIns *pc, TValue *base, struct vm_frame *vmf, BCIns ins);
void *uj_vm_callhook(BCIns *pc, TValue *base, struct vm_frame *vmf, BCIns ins);

#ifdef UJIT_CINTERP
void *uj_vm_cont_ra(BCIns *pc, TValue *base, struct vm_frame *vmf, BCIns ins,
		    TValue *results);
void *uj_vm_cont_nop(BCIns *pc, TValue *base, struct vm_frame *vmf, BCIns ins,
		     TValue *results);
void *uj_vm_cont_condt(BCIns *pc, TValue *base, struct vm_frame *vmf, BCIns ins,
		       TValue *results);
void *uj_vm_cont_condf(BCIns *pc, TValue *base, struct vm_frame *vmf, BCIns ins,
		       TValue *results);
void *uj_vm_cont_cat(BCIns *pc, TValue *base, struct vm_frame *vmf, BCIns ins,
		     TValue *results, TValue *caller_base);
#endif

typedef void *(*CInterpFunction)(BCIns *, TValue *, struct vm_frame *, BCIns);

extern const CInterpFunction uj_vm_bc_dispatch_cinterp[];

#endif /* !_UJ_VM_H */
