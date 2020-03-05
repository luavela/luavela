/*
 * Implementation of DWARF2 personality handler.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "lj_obj.h"
#include "lj_vm.h"
#include "uj_unwind_ext.h"
#include "uj_unwind.h"
#include "uj_throw.h"
#include "uj_errmsg.h"
#include "uj_cframe.h"
#ifndef NDEBUG
#include "uj_vmstate.h"
#endif /* !NDEBUG  */

#define UJ_EX_CODE_MASK 0xff

/*
 * uJIT uses external frame unwinding (EXT) for handling errors which uses the
 * system-provided unwind handler (see uj_unwind_ext.h for details).
 *
 * Caveats and features:
 *
 * - Setting up error handlers is zero-cost.
 *
 * - EXT requires unwind tables for *all* functions on the C stack between
 *   the pcall/catch and the error/throw. This is the default on x64,
 *   but needs to be manually enabled on x86/PPC for non-C++ code.
 *
 * - EXT provides full interoperability with C++ exceptions. You can throw
 *   Lua errors or C++ exceptions through a mix of Lua frames and C++ frames.
 *   C++ destructors are called as needed. C++ exceptions caught by pcall
 *   are converted to the string "C++ exception". Lua errors can be caught
 *   with catch (...) in C++.
 *
 * Here are a couple of portability considerations adapted from the original
 * implementation in LuaJIT:
 *
 * - When porting to a POSIX system with GCC and DWARF2 stack, *all* C code must
 *   be compiled with -funwind-tables (or -fexceptions). This includes uJIT
 *   itself, all of your C/Lua binding code, all loadable C modules and all C
 *   libraries that have callbacks which may be used to call back into Lua.
 *   C++ code must *not* be compiled with -fno-exceptions. Take a look at
 *   -DLUAJIT_UNWIND_EXTERNAL in LuaJIT if needed.
 *
 * - EXT cannot be enabled on WIN32 since system exceptions use code-driven SEH.
 *
 * - EXT is mandatory on WIN64 since the calling convention has an abundance
 *   of callee-saved registers (rbx, rbp, rsi, rdi, r12-r15, xmm6-xmm15).
 *
 * - EXT is mandatory on POSIX/x64 since the interpreter doesn't save r12/r13.
 */

/*
 * Check if the exception class received from the unwinder
 * denotes our platform-specific error code.
 */
static LJ_AINLINE int dwarf_is_our_exception(uint64_t uexclass)
{
	return (uexclass ^ UJ_UEXCLASS) <= UJ_EX_CODE_MASK;
}

/*
 * Dispatch the exception class received from the unwinder to
 * our platform-specific error code or to default_ex if the exception class
 * cannot be dispatched.
 */
static LJ_AINLINE int dwarf_dispatch_uexclass(uint64_t uexclass, int default_ex)
{
	if (dwarf_is_our_exception(uexclass))
		return (int)(uexclass & UJ_EX_CODE_MASK);

	return default_ex;
}

/* Search phase of the exception handling. */
static int dwarf_search(uint64_t uexclass, lua_State *L, const void *cframe)
{
	int ex = dwarf_dispatch_uexclass(uexclass, 0);

	if (uj_unwind_search(L, ex, cframe) == NULL)
		return _URC_CONTINUE_UNWIND;

	if (!dwarf_is_our_exception(uexclass))
		setstrV(L, L->top++, uj_errmsg_str(L, UJ_ERR_ERRCPP));

	return _URC_HANDLER_FOUND;
}

/* Cleanup phase of the exception handling. */
static int dwarf_cleanup(int actions, uint64_t uexclass,
			 struct _Unwind_Exception *uex,
			 struct _Unwind_Context *ctx, lua_State *L,
			 const void *cframe)
{
	int ex = dwarf_dispatch_uexclass(uexclass, LUA_ERRRUN);

	/*
	 * Handler frame that was detected during the search phase is reached
	 * again during the cleanup phase: Destroy exception object. Needed
	 * only for external exceptions, nothing to clean in our case.
	 */
	if (!dwarf_is_our_exception(uexclass) && (actions & _UA_HANDLER_FRAME))
		_Unwind_DeleteException(uex);

	if ((actions & _UA_FORCE_UNWIND))
		return _URC_CONTINUE_UNWIND;

	cframe = uj_unwind_cleanup(L, ex, cframe);
	if (cframe != NULL) {
		/* Reached landing pad to the user code. */
		_Unwind_SetGR(ctx, UJ_TARGET_EHRETREG, ex);
		_Unwind_SetIP(ctx, (uintptr_t)(uj_cframe_unwind_is_ff(cframe) ?
						       lj_vm_unwind_ff_eh :
						       lj_vm_unwind_c_eh));
		return _URC_INSTALL_CONTEXT;
	}

	if ((actions & _UA_HANDLER_FRAME)) {
		/*
		 * Workaround for an ancient libgcc bug.
		 * Still present in RHEL 5.5. :-/
		 * Real fix: http://gcc.gnu.org/viewcvs/trunk/gcc/unwind-dw2.c?r1=121165&r2=124837&pathrev=153877&diff_format=h
		 */
		_Unwind_SetGR(ctx, UJ_TARGET_EHRETREG, ex);
		_Unwind_SetIP(ctx, (uintptr_t)lj_vm_unwind_rethrow);
		return _URC_INSTALL_CONTEXT;
	}

	return _URC_CONTINUE_UNWIND;
}

/* DWARF2 personality handler referenced from the interpreter's .eh_frame. */
int uj_dwarf_personality(int version, int actions, uint64_t uexclass,
			 struct _Unwind_Exception *uex,
			 struct _Unwind_Context *ctx)
{
	lua_State *L;
	const void *cframe;

	if (version != 1)
		return _URC_FATAL_PHASE1_ERROR;

	cframe = (const void *)_Unwind_GetCFA(ctx);
	L = uj_cframe_L(cframe);

	lua_assert(uj_vmstate_get(&G(L)->vmstate) == UJ_VMST_INTERP);

	if ((actions & _UA_SEARCH_PHASE))
		return dwarf_search(uexclass, L, cframe);

	if ((actions & _UA_CLEANUP_PHASE))
		return dwarf_cleanup(actions, uexclass, uex, ctx, L, cframe);

	return _URC_CONTINUE_UNWIND;
}
