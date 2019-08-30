/*
 * Garbage collector.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _LJ_GC_H
#define _LJ_GC_H

#include "lj_obj.h"
#include "uj_obj_marks.h"

#define LJ_GC_WHITES    (LJ_GC_WHITE0 | LJ_GC_WHITE1)
#define LJ_GC_COLORS    (LJ_GC_WHITES | LJ_GC_BLACK)
#define LJ_GC_WEAK      (LJ_GC_WEAKKEY | LJ_GC_WEAKVAL)

/* Macros to test and set GCobj colors. */
#define iswhite(x)      ((x)->gch.marked & LJ_GC_WHITES)
#define isblack(x)      ((x)->gch.marked & LJ_GC_BLACK)
#define isgray(x)       (!((x)->gch.marked & (LJ_GC_BLACK|LJ_GC_WHITES)))
#define tviswhite(x)    (tvisgcv(x) && iswhite(gcV(x)))
#define otherwhite(g)   ((g)->gc.currentwhite ^ LJ_GC_WHITES)
#define isdead(g, v)    ((v)->gch.marked & otherwhite(g) & LJ_GC_WHITES)


#define curwhite(g)     ((g)->gc.currentwhite & LJ_GC_WHITES)
#define newwhite(g, x)  (obj2gco(x)->gch.marked = (uint8_t)curwhite(g))
#define makewhite(g, x) \
  ((x)->gch.marked = ((x)->gch.marked & (uint8_t)~LJ_GC_COLORS) | curwhite(g))
#define flipwhite(x)    ((x)->gch.marked ^= LJ_GC_WHITES)
#define black2gray(x)   ((x)->gch.marked &= (uint8_t)~LJ_GC_BLACK)
#define fixstring(s)    ((s)->marked |= LJ_GC_FIXED)
#define unfixstring(s)  ((s)->marked &= ~LJ_GC_FIXED)
#define markfinalized(x)        ((x)->gch.marked |= LJ_GC_FINALIZED)

static LJ_AINLINE void lj_gc_push(GCobj* o, GCobj** list) {
  o->gch.gclist = *list;
  *list = o;
}

/* Collector. */
size_t lj_gc_separateudata(global_State *g, int all);
void lj_gc_finalize_udata(lua_State *L);
#if LJ_HASFFI
void lj_gc_finalize_cdata(lua_State *L);
#else
#define lj_gc_finalize_cdata(L)         UNUSED(L)
#endif
void lj_gc_freeall(global_State *g);
int lj_gc_step(lua_State *L);
void lj_gc_step_fixtop(lua_State *L);
#if LJ_HASJIT
int lj_gc_step_jit(global_State *g, size_t steps);
#endif
void lj_gc_fullgc(lua_State *L);

/* GC check: drive collector forward if the GC threshold has been reached. */
#define lj_gc_check(L) \
  { if (LJ_UNLIKELY(uj_mem_total(MEM(L)) >= G(L)->gc.threshold)) \
      lj_gc_step(L); }
void lj_gc_check_fixtop(lua_State *L);

/* Write barriers. */
void lj_gc_barrierf(global_State *g, GCobj *o, GCobj *v);
void lj_gc_barrieruv(global_State *g, TValue *tv);
void lj_gc_closeuv(global_State *g, GCupval *uv);
#if LJ_HASJIT
void lj_gc_barriertrace(global_State *g, uint32_t traceno);
#endif

/* Move the GC propagation frontier back for tables (make it gray again). */
static LJ_AINLINE void lj_gc_barrierback(global_State *g, GCtab *t)
{
  GCobj *o = obj2gco(t);
  lua_assert(!uj_obj_is_sealed(o));
  lua_assert(isblack(o) && !isdead(g, o));
  lua_assert(g->gc.state != GCSfinalize && g->gc.state != GCSpause);
  black2gray(o);
  lj_gc_push(o, &g->gc.grayagain);
}

/* Barrier for stores to table objects. TValue and GCobj variant. */
#define lj_gc_anybarriert(L, t)  \
  { if (LJ_UNLIKELY(isblack(obj2gco(t)))) lj_gc_barrierback(G(L), (t)); }
#define lj_gc_barriert(L, t, tv) \
  { if (tviswhite(tv) && isblack(obj2gco(t))) \
      lj_gc_barrierback(G(L), (t)); }
#define lj_gc_objbarriert(L, t, o)  \
  { if (iswhite(obj2gco(o)) && isblack(obj2gco(t))) \
      lj_gc_barrierback(G(L), (t)); }

/* Barrier for stores to any other object. TValue and GCobj variant. */
#define lj_gc_barrier(L, p, tv) \
  { if (tviswhite(tv) && isblack(obj2gco(p))) \
      lj_gc_barrierf(G(L), obj2gco(p), gcV(tv)); }
#define lj_gc_objbarrier(L, p, o) \
  { if (iswhite(obj2gco(o)) && isblack(obj2gco(p))) \
      lj_gc_barrierf(G(L), obj2gco(p), obj2gco(o)); }

#endif
