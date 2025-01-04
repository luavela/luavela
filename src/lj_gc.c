/*
 * Garbage collector.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 *
 * Major portions taken verbatim or adapted from the Lua interpreter.
 * Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
 */

#include "lj_obj.h"
#include "uj_obj_seal.h"
#include "lj_gc.h"
#include "uj_dispatch.h"
#include "uj_throw.h"
#include "uj_str.h"
#include "uj_sbuf.h"
#include "lj_tab.h"
#include "uj_func.h"
#include "uj_proto.h"
#include "uj_upval.h"
#include "uj_udata.h"
#include "uj_meta.h"
#include "uj_state.h"
#include "lj_frame.h"
#if LJ_HASFFI
#include "ffi/lj_ctype.h"
#include "ffi/lj_cdata.h"
#endif
#include "jit/lj_trace.h"
#include "lj_vm.h"
#include "uj_strhash.h"

#define GCSTEPSIZE      1024u

/* Number of regular objects (either dead or alive) to traverse
** during GCSsweep phase in a single gc_onestep.
*/
#define GCSWEEPMAX      40
#define GCSWEEPCOST     10
#define GCFINALIZECOST  100

/* Macros to set GCobj colors and flags. */
#define white2gray(x)           ((x)->gch.marked &= (uint8_t)~LJ_GC_WHITES)
#define gray2black(x)           ((x)->gch.marked |= LJ_GC_BLACK)
#define isfinalized(u)          ((u)->marked & LJ_GC_FINALIZED)

/* -- Mark phase ---------------------------------------------------------- */

/* Mark a TValue (if needed). */
#define gc_marktv(g, tv) \
  { lua_assert(!tvisgcv(tv) || (~gettag(tv) == gcval(tv)->gch.gct)); \
    if (tviswhite(tv)) gc_mark(g, gcV(tv)); }

/* Mark a GCobj (if needed). */
#define gc_markobj(g, o) \
  { if (iswhite(obj2gco(o))) gc_mark(g, obj2gco(o)); }

/* Mark a string object. */
#define gc_mark_str(s)          ((s)->marked &= (uint8_t)~LJ_GC_WHITES)

/* Mark a white GCobj. */
static void gc_mark(global_State *g, GCobj *o) {
  int gct = o->gch.gct;
  if (LJ_UNLIKELY(uj_obj_is_sealed(o))) { return; }
  lua_assert(iswhite(o) && !isdead(g, o));
  white2gray(o);
  if (LJ_UNLIKELY(gct == ~LJ_TUDATA)) {
    GCtab *mt = gco2ud(o)->metatable;
    gray2black(o);  /* Userdata are never gray. */
    if (mt) { gc_markobj(g, mt); }
    gc_markobj(g, gco2ud(o)->env);
  } else if (LJ_UNLIKELY(gct == ~LJ_TUPVAL)) {
    GCupval *uv = gco2uv(o);
    gc_marktv(g, uvval(uv));
    if (uv->closed) {
      gray2black(o);  /* Closed upvalues are never gray. */
    }
  } else if (gct != ~LJ_TSTR && gct != ~LJ_TCDATA) {
    lua_assert(gct == ~LJ_TFUNC || gct == ~LJ_TTAB ||
               gct == ~LJ_TTHREAD || gct == ~LJ_TPROTO);
    lj_gc_push(o, &g->gc.gray);
  }
}

/* Mark GC roots. */
static void gc_mark_gcroot(global_State *g) {
  ptrdiff_t i;
  for (i = 0; i < GCROOT_MAX; i++) {
    if (g->gcroot[i] != NULL) {
      gc_markobj(g, g->gcroot[i]);
    }
  }
}

/* Start a GC cycle and mark the root set. */
static void gc_mark_start(global_State *g)
{
  g->gc.gray = NULL;
  g->gc.grayagain = NULL;
  g->gc.weak = NULL;
  gc_markobj(g, mainthread(g));
  gc_markobj(g, mainthread(g)->env);
  gc_marktv(g, &g->registrytv);
  gc_mark_gcroot(g);
  g->gc.state = GCSpropagate;
}

/* Mark open upvalues. */
static void gc_mark_uv(global_State *g) {
  GCupval *uv;
  for (uv = uvnext(&g->uvhead); uv != &g->uvhead; uv = uvnext(uv)) {
    lua_assert(uvprev(uvnext(uv)) == uv && uvnext(uvprev(uv)) == uv);
    if (isgray(obj2gco(uv))) {
      gc_marktv(g, uvval(uv));
    }
  }
}

/* Mark userdata in mmudata list. */
static void gc_mark_mmudata(global_State *g) {
  GCobj *root = g->gc.mmudata;
  GCobj *u = root;
  if (u) {
    do {
      u = gcnext(u);
      makewhite(g, u);  /* Could be from previous GC. */
      gc_mark(g, u);
    } while (u != root);
  }
}

/* Separate userdata objects to be finalized to mmudata list.
** Chain traversal stops at the first sealed object.
*/
size_t lj_gc_separateudata(global_State *g, int all) {
  size_t m = 0;
  GCobj **p = &mainthread(g)->nextgc;
  GCobj *o;
  while ((o = (*p)) != NULL) {
    if (LJ_UNLIKELY(uj_obj_is_sealed(o))) { break; }
    if (!(iswhite(o) || all) || isfinalized(gco2ud(o))) {
      p = &o->gch.nextgc; /* Nothing to do. */
    } else if (!uj_meta_lookup_mt(g, gco2ud(o)->metatable, MM_gc)) {
      markfinalized(o);  /* Done, as there's no __gc metamethod. */
      p = &o->gch.nextgc;
    } else { /* Otherwise move userdata to be finalized to mmudata list. */
      m += uj_udata_sizeof(gco2ud(o));
      markfinalized(o);
      *p = o->gch.nextgc;
      if (g->gc.mmudata != NULL) { /* Link to end of mmudata list. */
        GCobj *root = g->gc.mmudata;
        o->gch.nextgc = root->gch.nextgc;
        root->gch.nextgc = o;
        g->gc.mmudata = o;
      } else { /* Create circular list. */
        o->gch.nextgc = o;
        g->gc.mmudata = o;
      }
    }
  }
  return m;
}

/* -- Propagation phase --------------------------------------------------- */

/* Traverse a table. */
static int gc_traverse_tab(global_State *g, GCtab *t) {
  int weak = 0;
  const TValue *mode;
  GCtab *mt = t->metatable;
  if (mt) {
    gc_markobj(g, mt);
  }
  mode = uj_meta_lookup_mt(g, mt, MM_mode);
  if (mode && tvisstr(mode)) {  /* Valid __mode field? */
    const char *modestr = strVdata(mode);
    int c;
    while ((c = *modestr++)) {
      if (c == 'k') { weak |= LJ_GC_WEAKKEY; }
      else if (c == 'v') { weak |= LJ_GC_WEAKVAL; }
    }
    if (weak) {  /* Weak tables are cleared in the atomic phase. */
#if LJ_HASFFI
      CTState *cts = ctype_ctsG(g);
      if (cts && cts->finalizer == t) {
	weak = (int)(~0u & ~LJ_GC_WEAKVAL);
      } else
#endif
      {
	t->marked = (uint8_t)((t->marked & ~LJ_GC_WEAK) | weak);
	lj_gc_push(obj2gco(t), &g->gc.weak);
      }
    }
  }
  if (weak == LJ_GC_WEAK) { /* Nothing to mark if both keys/values are weak. */
    return 1;
  }
  if (!(weak & LJ_GC_WEAKVAL)) {  /* Mark array part. */
    size_t i, asize = t->asize;
    for (i = 0; i < asize; i++) {
      gc_marktv(g, arrayslot(t, i));
    }
  }
  if (t->hmask > 0) {  /* Mark hash part. */
    Node *node = t->node;
    size_t i, hmask = t->hmask;
    for (i = 0; i <= hmask; i++) {
      Node *n = &node[i];
      if (!tvisnil(&n->val)) {  /* Mark non-empty slot. */
        lua_assert(!tvisnil(&n->key));
        if (!(weak & LJ_GC_WEAKKEY)) { gc_marktv(g, &n->key); }
        if (!(weak & LJ_GC_WEAKVAL)) { gc_marktv(g, &n->val); }
      }
    }
  }
  return weak;
}

/* Traverse a function. */
static void gc_traverse_func(global_State *g, GCfunc *fn) {
  gc_markobj(g, fn->c.env);
  if (isluafunc(fn)) {
    uint32_t i;
    lua_assert(fn->l.nupvalues <= funcproto(fn)->sizeuv);
    gc_markobj(g, funcproto(fn));
    for (i = 0; i < fn->l.nupvalues; i++) { /* Mark Lua function upvalues. */
      gc_markobj(g, fn->l.uvptr[i]);
    }
  } else {
    uint32_t i;
    for (i = 0; i < fn->c.nupvalues; i++) { /* Mark C function upvalues. */
      gc_marktv(g, &fn->c.upvalue[i]);
    }
  }
}

#if LJ_HASJIT
/* Mark a trace. */
static void gc_marktrace(global_State *g, TraceNo traceno) {
  GCobj *o = obj2gco(traceref(G2J(g), traceno));
  lua_assert(traceno != G2J(g)->cur.traceno);
  if (iswhite(o)) {
    white2gray(o);
    lj_gc_push(o, &g->gc.gray);
  }
}

/* Traverse a trace. */
static void gc_traverse_trace(global_State *g, GCtrace *T) {
  IRRef ref;
  if (T->traceno == 0) return;
  for (ref = T->nk; ref < REF_TRUE; ref++) {
    IRIns *ir = &T->ir[ref];
    if (ir->o == IR_KGC) {
      gc_markobj(g, ir_kgc(ir));
    }
  }
  if (T->link) { gc_marktrace(g, T->link); }
  if (T->nextroot) { gc_marktrace(g, T->nextroot); }
  if (T->nextside) { gc_marktrace(g, T->nextside); }
  gc_markobj(g, T->startpt);
}

/* The current trace is a GC root while not anchored in the prototype (yet). */
#define gc_traverse_curtrace(g) gc_traverse_trace(g, &G2J(g)->cur)
#else
#define gc_traverse_curtrace(g) UNUSED(g)
#endif

/* Traverse a prototype. */
static void gc_traverse_proto(global_State *g, GCproto *pt) {
  ptrdiff_t i;
  gc_mark_str(proto_chunkname(pt));
  for (i = -(ptrdiff_t)pt->sizekgc; i < 0; i++) { /* Mark collectable consts. */
    gc_markobj(g, proto_kgc(pt, i));
  }
#if LJ_HASJIT
  if (pt->trace) { gc_marktrace(g, pt->trace); }
#endif
}

static const TValue *max_declared_frame_slot(const TValue *frame) {
  const GCfunc *fn   = frame_func(frame);
  const TValue *slot = frame;

  if (isluafunc(fn)) {
    slot += funcproto(fn)->framesize;
  }

  return slot;
}

/* Marks a single stack frame, i.e slots in the range [frame; top - 1]. */
static void gc_mark_stack_frame(lua_State *L, TValue *frame, const TValue *top) {
  global_State *g = G(L);
  TValue *slot;

  lua_assert(top - frame >= 1);

  if (!frame_isdummy(L, frame)) {
    /* Need to mark hidden function (but not L). */
    gc_markobj(g, frame_func(frame));
  }
  for (slot = frame + 1; slot < top; slot++) {
    gc_marktv(g, slot);
  }
}

/* Clears all unmarked slots above top - 1 by setting them to nil. */
static void gc_clear_upper_stack_slots(const lua_State *L) {
  TValue *slot;

  lua_assert(G(L)->gc.state == GCSatomic);

  for (slot = L->top; slot < L->stack + L->stacksize; slot++) {
    setnilV(slot);
  }
}

/* Traverse stack backwards frame by frame marking all slots on each iteration.
** NB! Extra vararg frame not skipped, marks function twice (harmless).
** Returns minimum needed stack size.
**/
static size_t gc_traverse_stack(lua_State *L) {
  TValue *frame = L->base - 1; /* link of the current frame */
  TValue *top   = L->top;      /* first slot above the current frame */
  const TValue *bottom   = L->stack;   /* absolute stack bottom */
  const TValue *max_slot = L->top - 1; /* max slot that *may* be used */

  lua_assert(L->top >= L->base);
  lua_assert(L->top > L->stack);
  lua_assert(frame_isdummy(L, L->stack));
  lua_assert(frame_isbottom(L->stack));

  for (;;) {
    const TValue *slot;
    gc_mark_stack_frame(L, frame, top); /* There is always at least 1 frame */

    if (frame == bottom) {
      break;
    }

    slot = max_declared_frame_slot(frame);
    if (slot > max_slot) {
      max_slot = slot;
    }
    top = !frame_iscont(frame)? frame : frame - 1; /* cont occupies 2 slots */
    frame = frame_prev(frame);
  }

  max_slot++; /* Correct bias of -1 (frame == base - 1). */
  if (max_slot > L->maxstack) {
    max_slot = L->maxstack;
  }
  return (size_t)(max_slot - bottom);
}

/* Traverse a thread object. */
static void gc_traverse_thread(global_State *g, lua_State *L) {
  size_t used = gc_traverse_stack(L);

  if (g->gc.state == GCSatomic) {
    gc_clear_upper_stack_slots(L);
  }

  gc_markobj(g, L->env);

  uj_state_stack_shrink(L, used);
}

/* Propagate one gray object. Traverse it and turn it black. */
static size_t propagatemark(global_State *g) {
  GCobj *o = g->gc.gray;
  int gct = o->gch.gct;
  lua_assert(isgray(o));
  gray2black(o);
  g->gc.gray = o->gch.gclist;  /* Remove from gray list. */
  if (LJ_LIKELY(gct == ~LJ_TTAB)) {
    GCtab *t = gco2tab(o);
    if (gc_traverse_tab(g, t) > 0) {
      black2gray(o);  /* Keep weak tables gray. */
    }
    return lj_tab_sizeof(t);
  } else if (LJ_LIKELY(gct == ~LJ_TFUNC)) {
    GCfunc *fn = gco2func(o);
    gc_traverse_func(g, fn);
    return uj_func_sizeof(fn);
  } else if (LJ_LIKELY(gct == ~LJ_TPROTO)) {
    GCproto *pt = gco2pt(o);
    gc_traverse_proto(g, pt);
    return uj_proto_sizeof(pt);
  } else if (LJ_LIKELY(gct == ~LJ_TTHREAD)) {
    lua_State *th = gco2th(o);
    lj_gc_push(o, &g->gc.grayagain);
    black2gray(o);  /* Threads are never black. */
    gc_traverse_thread(g, th);
    return sizeof(lua_State) + sizeof(TValue) * th->stacksize;
  } else {
#if LJ_HASJIT
    GCtrace *T = gco2trace(o);
    gc_traverse_trace(g, T);
    return lj_trace_sizeof(T);
#else
    lua_assert(0);
    return 0;
#endif
  }
}

/* Propagate all gray objects. */
static size_t gc_propagate_gray(global_State *g) {
  size_t m = 0;
  while (g->gc.gray != NULL) {
    m += propagatemark(g);
  }
  return m;
}

/* -- Sweep phase --------------------------------------------------------- */

/* Try to shrink some common data structures. */
static void gc_shrink(global_State *g, lua_State *L) {
  uj_strhash_shrink(gl_strhash(g), L);
  uj_sbuf_shrink_tmp(L);
}

/* Type of GC free functions. */
typedef void (*GCFreeFunc)(global_State *g, GCobj *o);

/* GC free functions for LJ_TSTR .. LJ_TUDATA. ORDER LJ_T */
static const GCFreeFunc gc_freefunc[] = {
  (GCFreeFunc)uj_str_free,
  (GCFreeFunc)uj_upval_free,
  (GCFreeFunc)uj_state_free,
  (GCFreeFunc)uj_proto_free,
  (GCFreeFunc)uj_func_free,
#if LJ_HASJIT
  (GCFreeFunc)lj_trace_free,
#else
  (GCFreeFunc)0,
#endif
#if LJ_HASFFI
  (GCFreeFunc)lj_cdata_free,
#else
  (GCFreeFunc)0,
#endif
  (GCFreeFunc)lj_tab_free,
  (GCFreeFunc)uj_udata_free
};

/* Full sweep of a GC list. */
#define gc_fullsweep(g, p)      gc_sweep(g, (p), LJ_MAX_MEM)

/* Partial sweep of a GC list.
** @param lim: number of objects (either dead or alive) to traverse
** Note that this should terminate when sealed object
** is encountered. In this case, end of the chain
** return is emulated.
*/
static GCobj** gc_sweep(global_State *g, GCobj **p, uint32_t lim) {
  /* Mask with other white and LJ_GC_FIXED. */
  int ow = otherwhite(g);
  GCobj *o;
  while ((o = (*p)) != NULL && lim-- > 0) {
    if (uj_obj_is_sealed(o)) {
      /* Emulate end of chain sweeping when sealed object is found. */
      p = &g->nullobj;
      break;
    }
    if (o->gch.gct == ~LJ_TTHREAD) { /* Need to sweep open upvalues, too. */
      gc_fullsweep(g, &gco2th(o)->openupval);
    }
    if (((o->gch.marked ^ LJ_GC_WHITES) & ow)) {  /* Black or current white? */
      lua_assert(!isdead(g, o) || (o->gch.marked & LJ_GC_FIXED));
      makewhite(g, o);  /* Value is alive, change to the current white. */
      p = &o->gch.nextgc;
    } else {  /* Otherwise value is dead, free it. */
      lua_assert(isdead(g, o) || g->gc.currentwhite == LJ_GC_WHITES);
      *p = o->gch.nextgc;
      if (o == g->gc.root) {
        g->gc.root = o->gch.nextgc;  /* Adjust list anchor. */
      }
      gc_freefunc[o->gch.gct - ~LJ_TSTR](g, o);
    }
  }
  return p;

}


/* Check whether we can clear a key or a value slot from a table. */
static int gc_mayclear(const TValue *o, int val) {
  if (!tvisgcv(o)) { /* Only collectable objects can be weak references. */
    return 0;
  }

  if (tvisstr(o)) {       /* But strings cannot be used as weak references. */
    gc_mark_str(strV(o)); /* And need to be marked. */
    return 0;
  }

  if (uj_obj_is_sealed(gcV(o))) {
    return 0; /* Sealed objects are not collected, cannot clear. */
  }

  if (iswhite(gcV(o))) {
    return 1; /* Object is about to be collected. */
  }

  if (tvisudata(o) && val && isfinalized(udataV(o))) {
    return 1; /* Finalized userdata is dropped only from values. */
  }

  return 0; /* Default: Cannot clear. */
}

/* Clear collected entries from weak tables. */
static void gc_clearweak(GCobj *o) {
  while (o) {
    GCtab *t = gco2tab(o);
    lua_assert((t->marked & LJ_GC_WEAK));
    if ((t->marked & LJ_GC_WEAKVAL)) {
      size_t i, asize = t->asize;
      for (i = 0; i < asize; i++) {
        /* Clear array slot when value is about to be collected. */
        TValue *tv = arrayslot(t, i);
        if (gc_mayclear(tv, 1)) { setnilV(tv); }
      }
    }
    if (t->hmask > 0) {
      Node *node = t->node;
      size_t i, hmask = t->hmask;
      for (i = 0; i <= hmask; i++) {
        Node *n = &node[i];
        /* Clear hash slot when key or value is about to be collected. */
        if (!tvisnil(&n->val) && (gc_mayclear(&n->key, 0) ||
                                  gc_mayclear(&n->val, 1))) {
          setnilV(&n->val);
        }
      }
    }
    o = t->gclist;
  }
}

/* Call a userdata or cdata finalizer. */
static void gc_call_finalizer(global_State *g, lua_State *L,
                              const TValue *mo, GCobj *o) {
  /* Save and restore lots of state around the __gc callback. */
  uint8_t oldh = hook_save(g);
  size_t oldt = g->gc.threshold;
  int errcode;
  TValue *top;
  lj_trace_abort(g);
  top = L->top;
  L->top = top + uj_mm_narg[MM_gc] + 1;
  hook_entergc(g);  /* Disable hooks and new traces during __gc. */
  g->gc.threshold = LJ_MAX_MEM;  /* Prevent GC steps. */
  uj_state_add_event(L, EXTEV_GC_FINALIZER);
  copyTV(L, top, mo);
  setgcV(L, top+1, o, ~o->gch.gct);
  errcode = lj_vm_pcall(L, top+1, 1+0, -1);  /* Stack: |mo|o| -> | */
  hook_restore(g, oldh);
  g->gc.threshold = oldt;  /* Restore GC threshold. */
  uj_state_remove_event(L, EXTEV_GC_FINALIZER);
  if (errcode) {
    uj_throw(L, errcode);  /* Propagate errors. */
  }
}

/* Finalize one userdata or cdata object from the mmudata list. */
static void gc_finalize(lua_State *L) {
  global_State *g = G(L);
  GCobj *o = gcnext(g->gc.mmudata);
  const TValue *mo;
  lua_assert(g->jit_L == NULL);  /* Must not be called on trace. */
  /* Unchain from list of userdata to be finalized. */
  if (o == g->gc.mmudata) {
    g->gc.mmudata = NULL;
  } else {
    g->gc.mmudata->gch.nextgc = o->gch.nextgc;
  }
#if LJ_HASFFI
  if (o->gch.gct == ~LJ_TCDATA) {
    TValue tmp, *tv;
    /* Add cdata back to the GC list and make it white. */
    o->gch.nextgc = g->gc.root;
    g->gc.root = o;
    makewhite(g, o);
    o->gch.marked &= (uint8_t)~LJ_GC_CDATA_FIN;
    /* Resolve finalizer. */
    setcdataV(L, &tmp, gco2cd(o));
    tv = lj_tab_set(L, ctype_ctsG(g)->finalizer, &tmp);
    if (!tvisnil(tv)) {
      g->gc.nocdatafin = 0;
      copyTV(L, &tmp, tv);
      setnilV(tv);  /* Clear entry in finalizer table. */
      gc_call_finalizer(g, L, &tmp, o);
    }
    return;
  }
#endif
  /* Add userdata back to the main userdata list and make it white. */
  o->gch.nextgc = mainthread(g)->nextgc;
  mainthread(g)->nextgc = o;
  makewhite(g, o);
  /* Resolve the __gc metamethod. */
  mo = uj_meta_lookup_mt(g, gco2ud(o)->metatable, MM_gc);
  if (mo) { gc_call_finalizer(g, L, mo, o); }
}

/* Finalize all userdata objects from mmudata list. */
void lj_gc_finalize_udata(lua_State *L) {
  while (G(L)->gc.mmudata != NULL) { gc_finalize(L); }
}

#if LJ_HASFFI
/* Finalize all cdata objects from finalizer table. */
void lj_gc_finalize_cdata(lua_State *L) {
  global_State *g = G(L);
  CTState *cts = ctype_ctsG(g);
  if (cts) {
    GCtab *t = cts->finalizer;
    Node *node = t->node;
    ptrdiff_t i;
    t->metatable = NULL;  /* Mark finalizer table as disabled. */
    for (i = (ptrdiff_t)t->hmask; i >= 0; i--) {
      if (!tvisnil(&node[i].val) && tviscdata(&node[i].key)) {
        GCobj *o = gcV(&node[i].key);
        TValue tmp;
        makewhite(g, o);
        o->gch.marked &= (uint8_t)~LJ_GC_CDATA_FIN;
        copyTV(L, &tmp, &node[i].val);
        setnilV(&node[i].val);
        gc_call_finalizer(g, L, &tmp, o);
      }
    }
  }
}
#endif

/* Free all remaining GC objects. */
void lj_gc_freeall(global_State *g) {
  size_t i, strmask;
  uj_strhash_t *strhash        = gl_strhash(g);
  uj_strhash_t *strhash_sealed = gl_strhash_sealed(g);
  /* Free everything. */
  g->gc.currentwhite = LJ_GC_WHITES;
  uj_obj_unseal_all(g);
  gc_fullsweep(g, &g->gc.root);

  strmask = strhash->mask;
  for (i = 0; i <= strmask; i++) { /* Free all string hash chains. */
    gc_fullsweep(g, &strhash->hash[i]);
  }
  if (!gl_datastate(g)) {
    /* Otherwise it's not ours to manage. */
    g->strhash_sweep = strhash_sealed;
    strmask = strhash_sealed->mask;
    for (i = 0; i <= strmask; i++) { /* Free all string hash chains. */
      gc_fullsweep(g, &strhash_sealed->hash[i]);
    }
  }
}

/* -- Collector ----------------------------------------------------------- */

/* Atomic part of the GC cycle, transitioning from mark to sweep phase. */
static void atomic(global_State *g, lua_State *L) {
  size_t udsize;

  gc_mark_uv(g);  /* Need to remark open upvalues (the thread may be dead). */
  gc_propagate_gray(g);  /* Propagate any left-overs. */

  g->gc.gray = g->gc.weak;  /* Empty the list of weak tables. */
  g->gc.weak = NULL;
  lua_assert(!iswhite(obj2gco(mainthread(g))));
  gc_markobj(g, L);  /* Mark running thread. */
  gc_traverse_curtrace(g);  /* Traverse current trace. */
  gc_mark_gcroot(g);  /* Mark GC roots (again). */
  gc_propagate_gray(g);  /* Propagate all of the above. */

  g->gc.gray = g->gc.grayagain;  /* Empty the 2nd chance list. */
  g->gc.grayagain = NULL;
  gc_propagate_gray(g);  /* Propagate it. */

  udsize = lj_gc_separateudata(g, 0);  /* Separate userdata to be finalized. */
  gc_mark_mmudata(g);  /* Mark them. */
  udsize += gc_propagate_gray(g);  /* And propagate the marks. */

  /* All marking done, clear weak tables. */
  gc_clearweak(g->gc.weak);

  /* Prepare for sweep phase. */
  g->gc.currentwhite = (uint8_t)otherwhite(g);  /* Flip current white. */
  flipwhite(obj2gco(&g->strempty_own));
  g->gc.sweep = &g->gc.root;
  g->gc.estimate = uj_mem_total(MEM_G(g)) - (size_t)udsize;  /* Initial estimate. */
}

/* GC state machine. Returns a cost estimate for each step performed. */
static size_t gc_onestep(lua_State *L) {
  global_State *g = G(L);
  g->gc.state_count[g->gc.state]++;
  switch (g->gc.state) {
  case GCSpause:
    gc_mark_start(g);  /* Start a new GC cycle by marking all GC roots. */
    return 0;
  case GCSpropagate:
    if (g->gc.gray != NULL) {
      return propagatemark(g);  /* Propagate one gray object. */
    }
    g->gc.state = GCSatomic;  /* End of mark phase. */
    return 0;
  case GCSatomic:
    if (g->jit_L != NULL) { /* Don't run atomic phase on trace. */
      return LJ_MAX_MEM;
    }
    atomic(g, L);
    g->gc.state = GCSsweepstring;  /* Start of sweep phase. */
    g->gc.sweepstr = 0;
    return 0;
  case GCSsweepstring: {
    size_t old = uj_mem_total(MEM(L));
    uj_strhash_t *strhash = gl_strhash(g);
    gc_fullsweep(g, &strhash->hash[g->gc.sweepstr++]);  /* Sweep one chain. */
    if (g->gc.sweepstr > strhash->mask) {
      g->gc.state = GCSsweep;  /* All string hash chains sweeped. */
    }
    lua_assert(old >= uj_mem_total(MEM(L)));
    g->gc.estimate -= old - uj_mem_total(MEM(L));
    return GCSWEEPCOST;
    }
  case GCSsweep: {
    size_t old = uj_mem_total(MEM(L));
    g->gc.sweep = gc_sweep(g, g->gc.sweep, GCSWEEPMAX);
    lua_assert(old >= uj_mem_total(MEM(L)));
    g->gc.estimate -= old - uj_mem_total(MEM(L));
    if (*g->gc.sweep == NULL) {
      gc_shrink(g, L);
      if (g->gc.mmudata != NULL) {  /* Need any finalizations? */
        g->gc.state = GCSfinalize;
#if LJ_HASFFI
        g->gc.nocdatafin = 1;
#endif
      } else {  /* Otherwise skip this phase to help the JIT. */
        g->gc.state = GCSpause;  /* End of GC cycle. */
        g->gc.debt = 0;
      }
    }
    return GCSWEEPMAX*GCSWEEPCOST;
    }
  case GCSfinalize:
    if (g->gc.mmudata != NULL) {
      if (g->jit_L != NULL) { /* Don't call finalizers on trace. */
        return LJ_MAX_MEM;
      }
      gc_finalize(L);  /* Finalize one userdata object. */
      if (g->gc.estimate > GCFINALIZECOST) {
        g->gc.estimate -= GCFINALIZECOST;
      }
      return GCFINALIZECOST;
    }
#if LJ_HASFFI
    if (!g->gc.nocdatafin) lj_tab_rehash(L, ctype_ctsG(g)->finalizer);
#endif
    g->gc.state = GCSpause;  /* End of GC cycle. */
    g->gc.debt = 0;
    return 0;
  default:
    lua_assert(0);
    return 0;
  }
}

/* Perform a limited amount of incremental GC steps. */
int lj_gc_step(lua_State *L) {
  size_t lim;
  global_State *g = G(L);
  struct vmstate_context vmsc;
  uj_vmstate_save(g->vmstate, &vmsc);
  uj_vmstate_set(&g->vmstate, UJ_VMST_GC);
  lim = (GCSTEPSIZE/100) * g->gc.stepmul;
  if (lim == 0) { lim = LJ_MAX_MEM; }
  if (uj_mem_total(MEM(L)) > g->gc.threshold) {
    g->gc.debt += uj_mem_total(MEM(L)) - g->gc.threshold;
  }
  do {
    lim -= gc_onestep(L);
    if (g->gc.state == GCSpause) {
      g->gc.threshold = (g->gc.estimate/100) * g->gc.pause;
      uj_vmstate_restore(&g->vmstate, &vmsc);
      return 1;  /* Finished a GC cycle. */
    }
  } while ((int32_t)lim > 0);
  if (g->gc.debt < GCSTEPSIZE) {
    g->gc.threshold = uj_mem_total(MEM(L)) + GCSTEPSIZE;
    uj_vmstate_restore(&g->vmstate, &vmsc);
    return -1;
  } else {
    g->gc.debt -= GCSTEPSIZE;
    g->gc.threshold = uj_mem_total(MEM(L));
    uj_vmstate_restore(&g->vmstate, &vmsc);
    return 0;
  }
}

/* Ditto, but fix the stack top first. */
static LJ_AINLINE void gc_step_fixtop(lua_State *L)
{
  uj_state_stack_sync_top(L);
  lj_gc_step(L);
}

void lj_gc_step_fixtop(lua_State *L) {
  gc_step_fixtop(L);
}

void lj_gc_check_fixtop(lua_State *L) {
  if (LJ_UNLIKELY(uj_mem_total(MEM(L)) >= G(L)->gc.threshold))
    gc_step_fixtop(L);
}

#if LJ_HASJIT
/* Perform multiple GC steps. Called from JIT-compiled code. */
int lj_gc_step_jit(global_State *g, size_t steps) {
  lua_State *L = g->jit_L;
  L->base = G(L)->jit_base;
  L->top = curr_topL(L);
  while (steps-- > 0 && lj_gc_step(L) == 0)
    ;
  /* Return 1 to force a trace exit. */
  return (G(L)->gc.state == GCSatomic || G(L)->gc.state == GCSfinalize);
}
#endif

/* Perform a full GC cycle. */
void lj_gc_fullgc(lua_State *L) {
  global_State *g = G(L);
  struct vmstate_context vmsc;
  uj_vmstate_save(g->vmstate, &vmsc);
  uj_vmstate_set(&g->vmstate, UJ_VMST_GC);
  if (g->gc.state <= GCSatomic) {  /* Caught somewhere in the middle. */
    g->gc.sweep = &g->gc.root;  /* Sweep everything (preserving it). */
    g->gc.gray = NULL;  /* Reset lists from partial propagation. */
    g->gc.grayagain = NULL;
    g->gc.weak = NULL;
    g->gc.state = GCSsweepstring;  /* Fast forward to the sweep phase. */
    g->gc.sweepstr = 0;
  }
  while (g->gc.state == GCSsweepstring || g->gc.state == GCSsweep) {
    gc_onestep(L);  /* Finish sweep. */
  }
  lua_assert(g->gc.state == GCSfinalize || g->gc.state == GCSpause);
  /* Now perform a full GC. */
  g->gc.state = GCSpause;
  do { gc_onestep(L); } while (g->gc.state != GCSpause);
  g->gc.threshold = (g->gc.estimate/100) * g->gc.pause;
  uj_vmstate_restore(&g->vmstate, &vmsc);
}

/* -- Write barriers ------------------------------------------------------ */

/* Move the GC propagation frontier forward. */
void lj_gc_barrierf(global_State *g, GCobj *o, GCobj *v) {
  lua_assert(isblack(o) && iswhite(v) && !isdead(g, v) && !isdead(g, o));
  lua_assert(g->gc.state != GCSfinalize && g->gc.state != GCSpause);
  lua_assert(o->gch.gct != ~LJ_TTAB);
  /* Preserve invariant during propagation. Otherwise it doesn't matter. */
  if (g->gc.state == GCSpropagate || g->gc.state == GCSatomic) {
    gc_mark(g, v);  /* Move frontier forward. */
  } else {
    makewhite(g, o);  /* Make it white to avoid the following barrier. */
  }
}

/* Specialized barrier for closed upvalue. Pass &uv->tv. */
void lj_gc_barrieruv(global_State *g, TValue *tv) {
#define TV2MARKED(x) \
  (*((uint8_t *)(x) - offsetof(GCupval, tv) + offsetof(GCupval, marked)))
  if (g->gc.state == GCSpropagate || g->gc.state == GCSatomic) {
    gc_mark(g, gcV(tv));
  } else {
    TV2MARKED(tv) = (TV2MARKED(tv) & (uint8_t)~LJ_GC_COLORS) | curwhite(g);
  }
#undef TV2MARKED
}

/* Close upvalue. Also needs a write barrier. */
void lj_gc_closeuv(global_State *g, GCupval *uv) {
  GCobj *o = obj2gco(uv);
  /* Copy stack slot to upvalue itself and point to the copy. */
  copyTV(mainthread(g), &uv->tv, uvval(uv));
  uv->v = &uv->tv;
  uv->closed = 1;
  o->gch.nextgc = g->gc.root;
  g->gc.root = o;
  if (isgray(o)) {  /* A closed upvalue is never gray, so fix this. */
    if (g->gc.state == GCSpropagate || g->gc.state == GCSatomic) {
      gray2black(o);  /* Make it black and preserve invariant. */
      if (tviswhite(&uv->tv)) {
        lj_gc_barrierf(g, o, gcV(&uv->tv));
      }
    } else {
      makewhite(g, o);  /* Make it white, i.e. sweep the upvalue. */
      lua_assert(g->gc.state != GCSfinalize && g->gc.state != GCSpause);
    }
  }
}

#if LJ_HASJIT
/* Mark a trace if it's saved during the propagation phase. */
void lj_gc_barriertrace(global_State *g, uint32_t traceno) {
  if (g->gc.state == GCSpropagate || g->gc.state == GCSatomic) {
    gc_marktrace(g, traceno);
  }
}
#endif
