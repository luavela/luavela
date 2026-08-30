/*
 * Temporary stubs for MacOS build.
 *
 * Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "lj_obj.h"
#include "uj_dispatch.h"
#include "uj_err.h"
#include "uj_vm.h"

const CInterpFunction uj_vm_bc_dispatch_cinterp[GG_LEN_DDISP] = {NULL};

void uj_vm_ff_landing_pad(void)
{
	abort();
}

void uj_vm_c_landing_pad(void)
{
	abort();
}

void *uj_vm_inshook(BCIns *pc, TValue *base, struct vm_frame *vmf, BCIns ins)
{
	UNUSED(pc);
	UNUSED(base);
	UNUSED(vmf);
	UNUSED(ins);
	abort();
}

void *uj_vm_rethook(BCIns *pc, TValue *base, struct vm_frame *vmf, BCIns ins)
{
	UNUSED(pc);
	UNUSED(base);
	UNUSED(vmf);
	UNUSED(ins);
	abort();
}

void *uj_vm_callhook(BCIns *pc, TValue *base, struct vm_frame *vmf, BCIns ins)
{
	UNUSED(pc);
	UNUSED(base);
	UNUSED(vmf);
	UNUSED(ins);
	abort();
}

void uj_vm_call(lua_State *L, TValue *base, int nres1)
{
	UNUSED(base);
	UNUSED(nres1);
	uj_err_msg_caller(L, "C interpreter is not available");
}
