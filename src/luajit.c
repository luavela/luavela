/*
 * Implementation of JIT engine control.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "lj_obj.h"
#include "luajit.h"
#include "uj_err.h"
#include "uj_proto.h"
#include "uj_dispatch.h"
#include "lj_frame.h"
#if LJ_HASJIT
#include "jit/lj_trace.h"
#endif /* LJ_HASJIT */

/* -- JIT mode setting ---------------------------------------------------- */

#if LJ_HASJIT
/* Set JIT mode for a single prototype. */
static void luajit_setptmode(struct global_State *g, struct GCproto *pt,
			     int mode)
{
	if (mode & LUAJIT_MODE_ON) { /* (Re-)enable JIT compilation. */
		uj_proto_enable_jit(pt); /* Unpatch all ILOOP etc. bytecodes. */
	} else {                     /* Flush and/or disable JIT compilation. */
		if (!(mode & LUAJIT_MODE_FLUSH))
			uj_proto_disable_jit(pt);
		lj_trace_flushproto(g, pt); /* Flush all traces of prototype. */
	}
}

/* Recursively set the JIT mode for all children of a prototype. */
static void luajit_setptmode_all(struct global_State *g, struct GCproto *pt,
				 int mode)
{
	ptrdiff_t i;
	if (!(pt->flags & PROTO_CHILD))
		return;
	for (i = -(ptrdiff_t)pt->sizekgc; i < 0; i++) {
		GCobj *o = proto_kgc(pt, i);
		if (o->gch.gct == ~LJ_TPROTO) {
			luajit_setptmode(g, gco2pt(o), mode);
			luajit_setptmode_all(g, gco2pt(o), mode);
		}
	}
}
#endif /* LJ_HASJIT */

/* Public API function: control the JIT engine. */
int luaJIT_setmode(struct lua_State *L, int idx, int mode)
{
	struct global_State *g = G(L);
	int mm = mode & LUAJIT_MODE_MASK;
#if LJ_HASJIT
	lj_trace_abort(g);  /* Abort recording on any state change. */
#endif
	/* Avoid pulling the rug from under our own feet. */
	if (g->hookmask & HOOK_GC)
		uj_err_caller(L, UJ_ERR_NOGCMM);
	switch (mm) {
#if LJ_HASJIT
	case LUAJIT_MODE_ENGINE:
		if (mode & LUAJIT_MODE_FLUSH) {
			lj_trace_flushall(L);
		} else {
			if (!(mode & LUAJIT_MODE_ON))
				G2J(g)->flags &= ~(uint32_t)JIT_F_ON;
#if UJIT_IPROF_ENABLED
			/*
			 * This check is necessary until trace profiling is not
			 * introduced
			 */
			else if (L->iprof)
				/* Don't turn on JIT while profiling */
				return 0;
#endif /* UJIT_IPROF_ENABLED */
			else if (G2J(g)->flags & JIT_F_SSE2)
				G2J(g)->flags |= (uint32_t)JIT_F_ON;
			else /* Don't turn on JIT without SSE2 support. */
				return 0;
			uj_dispatch_update(g);
		}
		break;
	case LUAJIT_MODE_FUNC:
	case LUAJIT_MODE_ALLFUNC:
	case LUAJIT_MODE_ALLSUBFUNC: {
		const TValue *tv;
		GCproto *pt;
		if (idx == 0)
			tv = frame_prev(L->base - 1);
		else if (idx > 0)
			tv = L->base + (idx - 1);
		else
			tv = L->top + idx;
		/* Cannot use funcV() for frame slot. */
		if ((idx == 0 || tvisfunc(tv)) && isluafunc(&gcval(tv)->fn))
			pt = funcproto(&gcval(tv)->fn);
		else if (tvisproto(tv))
			pt = protoV(tv);
		else
			return 0;  /* Failed. */
		if (mm != LUAJIT_MODE_ALLSUBFUNC)
			luajit_setptmode(g, pt, mode);
		if (mm != LUAJIT_MODE_FUNC)
			luajit_setptmode_all(g, pt, mode);
		break;
		}
	case LUAJIT_MODE_TRACE:
		if (!(mode & LUAJIT_MODE_FLUSH))
			return 0;  /* Failed. */
		lj_trace_flush(G2J(g), idx);
		break;
#else /* LJ_HASJIT */
	case LUAJIT_MODE_ENGINE:
	case LUAJIT_MODE_FUNC:
	case LUAJIT_MODE_ALLFUNC:
	case LUAJIT_MODE_ALLSUBFUNC:
		UNUSED(idx);
		if (mode & LUAJIT_MODE_ON)
			return 0;  /* Failed. */
		break;
#endif /* LJ_HASJIT */
	case LUAJIT_MODE_WRAPCFUNC:
		if (mode & LUAJIT_MODE_ON) {
			if (idx == 0)
				return 0;  /* Failed */

			const TValue *tv;
			tv  = idx > 0 ? L->base + (idx - 1) : L->top + idx;
			if (!tvislightud(tv))
				return 0;  /* Failed */

UJ_PEDANTIC_OFF /* casting void* to a function ptr */
			g->wrapf = (lua_CFunction)lightudV(tv);
UJ_PEDANTIC_ON

			g->bc_cfunc_ext = BCINS_AD(BC_FUNCCW, 0, 0);
		} else {
			g->bc_cfunc_ext = BCINS_AD(BC_FUNCC, 0, 0);
		}
		break;
	default:
		return 0;  /* Failed. */
	}
	return 1;  /* OK. */
}
