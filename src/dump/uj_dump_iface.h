/*
 * This module provides dump functions for debugging and ivestigation purposes.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJ_DUMP_IFACE_H
#define _UJ_DUMP_IFACE_H

#include <stdio.h>

#if LJ_HASJIT
#include "jit/lj_trace.h"
#endif /* LJ_HASJIT */

enum {
#define DUMP_PROGRESS_DEF(state, suffix) JSTATE_TRACE_##state,
#include "dump/uj_dump_progress.h"
#undef DUMP_PROGRESS_DEF
};

/*
 * Dump stack of the given state assuming that its layout is not corrupted
 * (from top to bottom, see uj_state.c for more details):
 * | substack for metamethods |
 * |>maxstack=================|
 * | free slots               |
 * |>top======================|
 * | frames                   |
 * |>bottom===================|
 */
void uj_dump_stack(FILE *out, const lua_State *L);

/*
 * Dump to output a single bytecode instruction pointed to by ins of the
 * prototype pt. If nest_level > 0, corresponding padding is written between
 * instruction number and its mnemonic.
 * Per-instruction dumping is used in trace recording dumps.
 */
void uj_dump_bc_ins(FILE *out, const GCproto *pt, const BCIns *ins,
		    uint8_t nest_level);

/*
 * Dump to output "pseudo-bytecode" for a non-Lua function (i.e. a C function or
 * a fast function). If nest_level > 0, corresponding padding is written between
 * instruction number and its mnemonic. This interface is used for consistent
 * dumping in trace recording dumps.
 */
void uj_dump_nonlua_bc_ins(FILE *out, const GCfunc *func, uint8_t nest_level);

/*
 * Dump all bytecode instructions of the function to output. If func is not a
 * Lua function (ergo does not have a bytecode), a brief function description
 * is printed instead.
 */
void uj_dump_bc(FILE *out, const GCfunc *func);

/*
 * Dumps bytecode of 'func' and prints source code similar to gdb's "disas /s"
 * Also highlights a bytecode instruction at 'hl_bc_pos' with '->'
 * If bc_hl_pos == NO_BCPOS, no instruction gets highlighted
 */
void uj_dump_bc_and_source(FILE *out, const GCfunc *func, BCPos hl_bc_pos);

#if LJ_HASJIT
/*
 * Dump trace IR to output. Can be configured additionally to dump interleaved
 * snapshots (on by default) and allocated host registers (on by default).
 * Configuration is compile-time (see the implementation file for details).
 */
void uj_dump_ir(FILE *out, const GCtrace *trace);

/*
 * Dump trace mcode to output.
 */
void uj_dump_mcode(FILE *out, const jit_State *J, const GCtrace *trace);

/*
 *
 * Interfaces for dumping compiler's progress dynamically
 *
 */

/*
 * Start dumping compiler's progress corresponding to coroutine L to out.
 * out will be cached internally for subsequent progress dumping.
 * Returns 0 on success and non-0 otherwise (e.g. dumping is already started).
 */
int uj_dump_start(const lua_State *L, FILE *out);

/*
 * Stop dumping compiler's progress corresponding to coroutine L. If dumping
 * was started to a regular file (i.e. not stdout or stderr), corresponding
 * descriptor will be closed. Returns 0 on success and non-0 otherwise
 * (e.g. no dumping was actually run).
 */
int uj_dump_stop(const lua_State *L);

/*
 * Dump event jstate of the compiler J with extra opaque data. This interface
 * is supposed to be used only by platform guts, no reason to proxy it via
 * Lua bindings to the outer world.
 */
void uj_dump_compiler_progress(int jstate, const jit_State *J, void *data);

#endif /* LJ_HASJIT */

#endif /* !_UJ_DUMP_IFACE_H */
