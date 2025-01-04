/*
 * uJIT VM states.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _UJ_VMSTATE_H
#define _UJ_VMSTATE_H

#include "lj_def.h"

/*
 * VM states generating macro.
 *
 * PROFILER: In enum below, the states starting from IDLE VM state
 * are host VM states.
 * These are the states which don't represent any state of guest code execution
 * i.e. when some internal VM processing/housekeeping is being carried out
 * (opposed to e.g. FUNC states when some guest function is being executed).
 * In terms of profiling, for such states only RIP value data is streamed.
 */
#define VMSTATEDEF(_) \
	_(LFUNC)  /* Interpreter: Executing plain Lua code.        */ \
	_(FFUNC)  /* Interpreter: Executing a fast function.       */ \
	_(CFUNC)  /* Interpreter: Executing registered C function. */ \
	_(IDLE)   /* Idle.                                         */ \
	_(INTERP) /* Interpreter: Switch between callable objects. */ \
	_(GC)     /* Garbage collector.  */ \
	_(EXIT)   /* Trace exit handler. */ \
	_(RECORD) /* Trace recorder.     */ \
	_(OPT)    /* Optimizer.          */ \
	_(ASM)    /* Assembler.          */ \

/*
 * PROFILER: INTERP state is entered each time interpreter goes from one
 * callable object to another: During these transitions, interpreter's base
 * is kinda unstable, and it's safer to use a separate state signalling that
 * it should not be touched at all. LFUNC/FFUNC/CFUNC state will be entered
 * as soon as appropriate. Example INTERP situations:
 *  * Resuming a coroutine
 *  * Executing BC_CALL
 *  * Executing BC_RET
 *  * ...see comprehensive list in the VM codebase
 */

/* VM states. */
enum vmstate {
#define VMSTATEENUM(name) UJ_VMST_##name,
	VMSTATEDEF(VMSTATEENUM)
#undef VMSTATEENUM
	UJ_VMST__MAX,
};

/*
 * PROFILER HACK: VM is inside a trace. This is a pseudo-state used by profiler.
 * In fact, when VM executes a trace, vmstate is set to the trace number, but
 * we aggregate all such cases into one VM state during per-VM state profiling.
 */
#define UJ_VMST_TRACE (UJ_VMST__MAX)

/*
 * First host VM state num.
 * It is deliberately represented as the _next_ element after non-host VM states
 * in enum (not an absolute num).
 */
#define UJ_VMST_HVMST_START (UJ_VMST_CFUNC + 1)

/* PROFILER: Host VM states amount. */
#define UJ_VMST_HVMST_NUM (UJ_VMST__MAX - UJ_VMST_HVMST_START)

LJ_DATA const char *const uj_vmstate_names[];

/*
 * VM state context: The state itself plus any data this state depends on
 * (currently needed by profiler only).
 */

typedef int32_t vmstate_t;

struct vmstate_context {
	vmstate_t vmstate; /* State of the VM. */
};

/* Retrieve current VM state. */
static LJ_AINLINE enum vmstate uj_vmstate_get(const volatile vmstate_t *st)
{
	return ~(*st);
}

/* Set VM state to vmst. */
static LJ_AINLINE void uj_vmstate_set(volatile vmstate_t *st, enum vmstate vmst)
{
	*st = ~(vmstate_t)vmst;
}

/* Save VM state to ctx. */
static LJ_AINLINE void uj_vmstate_save(vmstate_t st,
				       struct vmstate_context *ctx)
{
	ctx->vmstate = st;
}

/* Restore VM state from ctx. */
static LJ_AINLINE void uj_vmstate_restore(volatile vmstate_t *st,
					  const struct vmstate_context *ctx)
{
	*st = ctx->vmstate;
}

#endif /* !_UJ_VMSTATE_H */
