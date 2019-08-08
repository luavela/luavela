/*
 * Trace management.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _LJ_TRACE_H
#define _LJ_TRACE_H

#include "lj_obj.h"

#if LJ_HASJIT
#include "jit/lj_jit.h"

/* Trace errors. */
typedef enum {
#define TREDEF(name, msg)       LJ_TRERR_##name,
#include "jit/lj_traceerr.h"
  LJ_TRERR__MAX
} TraceError;

LJ_NORET void lj_trace_err(jit_State *J, TraceError e);
LJ_NORET void lj_trace_err_info_func(jit_State *J, TraceError e);
LJ_NORET void lj_trace_err_info_op(jit_State *J, TraceError e, int32_t op);

/* Trace management. */
size_t lj_trace_sizeof(GCtrace *T);
void lj_trace_free(global_State *g, GCtrace *T);
void lj_trace_flushproto(global_State *g, GCproto *pt);
void lj_trace_flush(jit_State *J, TraceNo traceno);
int lj_trace_flushall(lua_State *L);
void lj_trace_initstate(global_State *g);
void lj_trace_freestate(global_State *g);

/* Event handling. */
void lj_trace_ins(jit_State *J, const BCIns *pc);
void lj_trace_hot(jit_State *J, const BCIns *pc);
int lj_trace_exit(jit_State *J, void *exptr);

/* Signal asynchronous abort of trace or end of trace. */
void lj_trace_abort(global_State *g);
#define lj_trace_end(J)         ((J)->state = LJ_TRACE_END)

#else

#define lj_trace_flushall(L)    (UNUSED(L), 0)
#define lj_trace_initstate(g)   UNUSED(g)
#define lj_trace_freestate(g)   UNUSED(g)
#define lj_trace_abort(g)       UNUSED(g)
#define lj_trace_end(J)         UNUSED(J)

#endif

#endif
