/*
 * Snapshot handling.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "lj_obj.h"

#if LJ_HASJIT

#include "uj_mem.h"
#include "lj_tab.h"
#include "uj_state.h"
#include "lj_frame.h"
#include "jit/lj_ir.h"
#include "jit/lj_jit.h"
#include "jit/lj_iropt.h"
#include "jit/lj_trace.h"
#include "jit/lj_snap.h"
#include "jit/lj_target.h"
#if LJ_HASFFI
#include "ffi/lj_ctype.h"
#include "ffi/lj_cdata.h"
#endif
#include "uj_cframe.h"

/* A really naive Bloom filter. But sufficient for our needs. */
typedef uintptr_t BloomFilter;
#define BLOOM_MASK      (8*sizeof(BloomFilter) - 1)
#define bloombit(x)     ((uintptr_t)1 << ((x) & BLOOM_MASK))
#define bloomset(b, x)  ((b) |= bloombit((x)))
#define bloomtest(b, x) ((b) & bloombit((x)))

/* Pass IR on to next optimization in chain (FOLD). */
#define emitir(ot, a, b)        (lj_ir_set(J, (ot), (a), (b)), lj_opt_fold(J))

/* Emit raw IR without passing through optimizations. */
#define emitir_raw(ot, a, b)    (lj_ir_set(J, (ot), (a), (b)), lj_ir_emit(J))

/* -- Snapshot buffer allocation ------------------------------------------ */

/* Grow snapshot buffer. */
void lj_snap_grow_buf_(jit_State *J, size_t need)
{
  size_t maxsnap = (size_t)J->param[JIT_P_maxsnap];
  if (need > maxsnap)
    lj_trace_err(J, LJ_TRERR_SNAPOV);
  J->snapbuf = (SnapShot *)uj_mem_grow(J->L, J->snapbuf,
                                    &(J->sizesnap), maxsnap, sizeof(SnapShot));
  J->cur.snap = J->snapbuf;
}

/* Grow snapshot map buffer. */
void lj_snap_grow_map_(jit_State *J, size_t need)
{
  if (need < 2*J->sizesnapmap)
    need = 2*J->sizesnapmap;
  else if (need < 64)
    need = 64;
  J->snapmapbuf = (SnapEntry *)uj_mem_realloc(J->L, J->snapmapbuf,
                    J->sizesnapmap*sizeof(SnapEntry), need*sizeof(SnapEntry));
  J->cur.snapmap = J->snapmapbuf;
  J->sizesnapmap = need;
}

/* -- Snapshot generation ------------------------------------------------- */

/* Add all modified slots to the snapshot. */
static size_t snapshot_slots(jit_State *J, SnapEntry *map, BCReg nslots)
{
  IRRef retf = J->chain[IR_RETF];  /* Limits SLOAD restore elimination. */
  BCReg s;
  size_t n = 0;
  for (s = 0; s < nslots; s++) {
    TRef tr = J->slot[s];
    IRRef ref = tref_ref(tr);
    if (ref) {
      SnapEntry sn = SNAP_TR(s, tr);
      IRIns *ir = &J->cur.ir[ref];
      if (!(sn & (SNAP_CONT|SNAP_FRAME)) &&
          ir->o == IR_SLOAD && ir->op1 == s && ref > retf) {
        /* No need to snapshot unmodified non-inherited slots. */
        if (!(ir->op2 & IRSLOAD_INHERIT))
          continue;
        /* No need to restore readonly slots and unmodified non-parent slots. */
        if ((ir->op2 & (IRSLOAD_READONLY|IRSLOAD_PARENT)) != IRSLOAD_PARENT)
          sn |= SNAP_NORESTORE;
      }
      map[n++] = sn;
    }
  }
  return n;
}

/* Add frame links at the end of the snapshot. */
static BCReg snapshot_framelinks(jit_State *J, SnapEntry *map)
{
  const TValue *frame = J->L->base - 1;
  const TValue *lim = J->L->base - J->baseslot;
  const TValue *ftop = frame + funcproto(frame_func(frame))->framesize;
  size_t f = 0;
  snap_store_pc(&map[f], J->pc); f += 2; /* The current PC is always the first entry. */
  while (frame > lim) {  /* Backwards traversal of all frames above base. */
    if (frame_islua(frame)) {
      snap_store_pc(&map[f], frame_pc(frame)); f += 2;
      frame = frame_prevl(frame);
    } else if (frame_iscont(frame)) {
      snap_store_ftsz(&map[f], frame_ftsz(frame)); f += 2;
      snap_store_pc(&map[f], frame_contpc(frame)); f += 2;
      frame = frame_prevd(frame);
    } else {
      lua_assert(!frame_isc(frame));
      snap_store_ftsz(&map[f], frame_ftsz(frame)); f += 2;
      frame = frame_prevd(frame);
      continue;
    }
    if (frame + funcproto(frame_func(frame))->framesize > ftop)
      ftop = frame + funcproto(frame_func(frame))->framesize;
  }
  lua_assert(f == (size_t)(2*(1 + J->framedepth)));
  return (BCReg)(ftop - lim);
}

/* Take a snapshot of the current stack. */
static void snapshot_stack(jit_State *J, SnapShot *snap, size_t nsnapmap)
{
  BCReg nslots = J->baseslot + J->maxslot;
  size_t nent;
  SnapEntry *p;
  /* Conservative estimate. */
  lj_snap_grow_map(J, nsnapmap + nslots + 2*((size_t)J->framedepth+1));
  p = &J->cur.snapmap[nsnapmap];
  nent = snapshot_slots(J, p, nslots);
  snap->topslot = (uint8_t)snapshot_framelinks(J, p + nent);
  snap->mapofs = (uint32_t)nsnapmap;
  snap->ref = (IRRef1)J->cur.nins;
  snap->nent = (uint8_t)nent;
  snap->nslots = (uint8_t)nslots;
  snap->count = 0;
  J->cur.nsnapmap = (uint32_t)(nsnapmap + nent + 2*(1 + J->framedepth));
}

/* Add or merge a snapshot. */
void lj_snap_add(jit_State *J)
{
  size_t nsnap = J->cur.nsnap;
  size_t nsnapmap = J->cur.nsnapmap;
  /* Merge if no ins. inbetween or if requested and no guard inbetween. */
  if (J->mergesnap ? !irt_isguard(J->guardemit) :
      (nsnap > 0 && J->cur.snap[nsnap-1].ref == J->cur.nins)) {
    if (nsnap == 1) {  /* But preserve snap #0 PC. */
      emitir_raw(IRT(IR_NOP, IRT_NIL), 0, 0);
      goto nomerge;
    }
    nsnapmap = J->cur.snap[--nsnap].mapofs;
  } else {
  nomerge:
    lj_snap_grow_buf(J, nsnap+1);
    J->cur.nsnap = (uint16_t)(nsnap+1);
  }
  J->mergesnap = 0;
  J->guardemit.irt = 0;
  snapshot_stack(J, &J->cur.snap[nsnap], nsnapmap);
}

/* -- Snapshot modification ----------------------------------------------- */

#define SNAP_USEDEF_SLOTS       (LJ_MAX_JSLOTS+LJ_STACK_EXTRA)

/* Find unused slots with reaching-definitions bytecode data-flow analysis. */
static BCReg snap_usedef(jit_State *J, uint8_t *udf,
                         const BCIns *pc, BCReg maxslot)
{
  BCReg s;
  GCobj *o;

  if (maxslot == 0) return 0;
#ifdef UJIT_USE_VALGRIND
  /* Avoid errors for harmless reads beyond maxslot. */
  memset(udf, 1, SNAP_USEDEF_SLOTS);
#else
  memset(udf, 1, maxslot);
#endif

  /* Treat open upvalues as used. */
  o = J->L->openupval;
  while (o) {
    if (uvval(gco2uv(o)) < J->L->base) break;
    udf[uvval(gco2uv(o)) - J->L->base] = 0;
    o = o->gch.nextgc;
  }

#define USE_SLOT(s)             udf[(s)] &= ~1
#define DEF_SLOT(s)             udf[(s)] *= 3

  /* Scan through following bytecode and check for uses/defs. */
  lua_assert(pc >= proto_bc(J->pt) && pc < proto_bc(J->pt) + J->pt->sizebc);
  for (;;) {
    BCIns ins = *pc++;
    BCOp op = bc_op(ins);
    switch (bcmode_b(op)) {
    case BCMvar:
      USE_SLOT(bc_b(ins));
      break;
    case BCMbase:
      lua_assert(op == BC_CAT); /* Handled in C operand part */
      break;
    case BCMlit:
      lua_assert(bcmode_c(op) == BCMlit); /* Handled in C operand part */
      break;
    case BCMnone:
      break;
    default:
      lua_assert(0); /* Unreachable */
      break;
    }
    switch (bcmode_c(op)) {
    case BCMvar: USE_SLOT(bc_c(ins)); break;
    case BCMbase:
      if (op == BC_KNIL)
        break; /* Handled in A operand part */
      lua_assert(op == BC_CAT);
      for (s = bc_b(ins); s <= bc_c(ins); s++) USE_SLOT(s);
      for (; s < maxslot; s++) DEF_SLOT(s);
      break;
    case BCMjump:
    handle_jump: {
      BCReg minslot = bc_a(ins);
      if (op >= BC_FORI && op <= BC_JFORL)
        minslot += FORL_EXT;
      else if (bc_isiterl(op) || bc_isitrnl(op))
        minslot += bc_b(pc[-2])-1;
      else if (op == BC_UCLO) {
        pc += bc_j(ins);
        break;
      }
      for (s = minslot; s < maxslot; s++) DEF_SLOT(s);
      return minslot < maxslot ? minslot : maxslot;
      }
    case BCMlit:
      if (bc_isjloop(op)) {
        goto handle_jump;
      } else if (bc_isret(op)) {
        BCReg top = op == BC_RETM ? maxslot : (bc_a(ins) + bc_d(ins)-1);
        for (s = 0; s < bc_a(ins); s++) DEF_SLOT(s);
        for (; s < top; s++) USE_SLOT(s);
        for (; s < maxslot; s++) DEF_SLOT(s);
        return 0;
      }
      break;
    case BCMfunc:
      return maxslot;  /* NYI: will abort, anyway. */
    case BCMstr:
    case BCMnum:
    case BCMpri:
    case BCMcdata:
    case BCMlits:
    case BCMuv:
    case BCMtab:
    case BCMnone:
      break;
    default:
      lua_assert(0); /* Unreachable */
      break;
    }
    switch (bcmode_a(op)) {
    case BCMvar: USE_SLOT(bc_a(ins)); break;
    case BCMdst:
       if (!(op == BC_ISTC || op == BC_ISFC)) DEF_SLOT(bc_a(ins));
       break;
    case BCMbase:
      if (op >= BC_CALLM && op <= BC_VARG) {
        BCReg top = (op == BC_CALLM || op == BC_CALLMT || bc_c(ins) == 0) ?
                    maxslot : (bc_a(ins) + bc_c(ins));
        s = bc_a(ins) - ((op == BC_ITERC || op == BC_ITERN) ? 3 : 0);
        for (; s < top; s++) USE_SLOT(s);
        for (; s < maxslot; s++) DEF_SLOT(s);
        if (op == BC_CALLT || op == BC_CALLMT) {
          for (s = 0; s < bc_a(ins); s++) DEF_SLOT(s);
          return 0;
        }
      } else if (op == BC_KNIL) {
        for (s = bc_a(ins); s <= bc_d(ins); s++) DEF_SLOT(s);
      } else if (op == BC_TSETM) {
        for (s = bc_a(ins)-1; s < maxslot; s++) USE_SLOT(s);
      }
      break;
    case BCMrbase:
      /* Handled in C operand part */
      lua_assert(bcmode_c(op) == BCMjump || bcmode_c(op) == BCMlit ||
        (op >= BC_FUNCF && op <= BC_FUNCCW)); /* or is function header */
      break;
    case BCMuv:
    case BCMnone:
      break;
    default:
      lua_assert(0); /* Unreachable */
      break;
    }
    lua_assert(pc >= proto_bc(J->pt) && pc < proto_bc(J->pt) + J->pt->sizebc);
  }

#undef USE_SLOT
#undef DEF_SLOT

  return 0;  /* unreachable */
}

/* Purge dead slots before the next snapshot. */
void lj_snap_purge(jit_State *J)
{
  uint8_t udf[SNAP_USEDEF_SLOTS];
  BCReg maxslot = J->maxslot;
  BCReg s = snap_usedef(J, udf, J->pc, maxslot);
  for (; s < maxslot; s++)
    if (udf[s] != 0)
      J->base[s] = 0;  /* Purge dead slots. */
}

/* Shrink last snapshot. */
void lj_snap_shrink(jit_State *J)
{
  SnapShot *snap = &J->cur.snap[J->cur.nsnap-1];
  SnapEntry *map = &J->cur.snapmap[snap->mapofs];
  size_t n, m, nlim, nent = snap->nent;
  uint8_t udf[SNAP_USEDEF_SLOTS];
  BCReg maxslot = J->maxslot;
  BCReg minslot = snap_usedef(J, udf, snap_pc(&map[nent]), maxslot);
  BCReg baseslot = J->baseslot;
  maxslot += baseslot;
  minslot += baseslot;
  snap->nslots = (uint8_t)maxslot;
  for (n = m = 0; n < nent; n++) {  /* Remove unused slots from snapshot. */
    BCReg s = snap_slot(map[n]);
    if (s < minslot || (s < maxslot && udf[s-baseslot] == 0))
      map[m++] = map[n];  /* Only copy used slots. */
  }
  snap->nent = (uint8_t)m;
  nlim = J->cur.nsnapmap - snap->mapofs - 1;
  /* TODO: ??? */
  while (n <= nlim) map[m++] = map[n++];  /* Move PC + frame links down. */
  J->cur.nsnapmap = (uint32_t)(snap->mapofs + m);  /* Free up space in map. */
}

/* -- Snapshot access ----------------------------------------------------- */

/* Initialize a Bloom Filter with all renamed refs.
** There are very few renames (often none), so the filter has
** very few bits set. This makes it suitable for negative filtering.
*/
static BloomFilter snap_renamefilter(GCtrace *T, SnapNo lim)
{
  BloomFilter rfilt = 0;
  IRIns *ir;
  for (ir = &T->ir[T->nins-1]; ir->o == IR_RENAME; ir--)
    if (ir->op2 <= lim)
      bloomset(rfilt, ir->op1);
  return rfilt;
}

/* Process matching renames to find the original RegSP. */
static RegSP snap_renameref(GCtrace *T, SnapNo lim, IRRef ref, RegSP rs)
{
  IRIns *ir;
  for (ir = &T->ir[T->nins-1]; ir->o == IR_RENAME; ir--)
    if (ir->op1 == ref && ir->op2 <= lim)
      rs = ir->prev;
  return rs;
}

/* Copy RegSP from parent snapshot to the parent links of the IR. */
IRIns *lj_snap_regspmap(GCtrace *T, SnapNo snapno, IRIns *ir)
{
  SnapShot *snap = &T->snap[snapno];
  SnapEntry *map = &T->snapmap[snap->mapofs];
  BloomFilter rfilt = snap_renamefilter(T, snapno);
  size_t n = 0;
  IRRef ref = 0;
  for ( ; ; ir++) {
    uint32_t rs;
    if (ir->o == IR_SLOAD) {
      if (!(ir->op2 & IRSLOAD_PARENT)) break;
      for ( ; ; n++) {
        lua_assert(n < snap->nent);
        if (snap_slot(map[n]) == ir->op1) {
          ref = snap_ref(map[n++]);
          break;
        }
      }
    } else if (ir->o == IR_PVAL) {
      ref = ir->op1 + REF_BIAS;
    } else {
      break;
    }
    rs = T->ir[ref].prev;
    if (bloomtest(rfilt, ref))
      rs = snap_renameref(T, snapno, ref, rs);
    ir->prev = (uint16_t)rs;
    lua_assert(regsp_used(rs));
  }
  return ir;
}

/* -- Snapshot replay ----------------------------------------------------- */

/* Replay constant from parent trace. */
static TRef snap_replay_const(jit_State *J, IRIns *ir)
{
  /* Only have to deal with constants that can occur in stack slots. */
  switch ((IROp)ir->o) {
  case IR_KPRI: return TREF_PRI(irt_type(ir->t));
  case IR_KINT: return lj_ir_kint(J, ir->i);
  case IR_KGC: return lj_ir_kgc(J, ir_kgc(ir), irt_t(ir->t));
  case IR_KNUM: return lj_ir_k64(J, IR_KNUM, ir_knum(ir));
  case IR_KINT64: return lj_ir_k64(J, IR_KINT64, ir_kint64(ir));
  case IR_KPTR: return lj_ir_kptr(J, ir_kptr(ir));  /* Continuation. */
  default: lua_assert(0); return TREF_NIL; break;
  }
}

/* De-duplicate parent reference. */
static TRef snap_dedup(jit_State *J, SnapEntry *map, size_t nmax, IRRef ref)
{
  size_t j;
  for (j = 0; j < nmax; j++)
    if (snap_ref(map[j]) == ref)
      return J->slot[snap_slot(map[j])] & ~(SNAP_CONT|SNAP_FRAME);
  return 0;
}

/* Emit parent reference with de-duplication. */
static TRef snap_pref(jit_State *J, GCtrace *T, SnapEntry *map, size_t nmax,
                      BloomFilter seen, IRRef ref)
{
  IRIns *ir = &T->ir[ref];
  TRef tr;
  if (irref_isk(ref))
    tr = snap_replay_const(J, ir);
  else if (!regsp_used(ir->prev))
    tr = 0;
  else if (!bloomtest(seen, ref) || (tr = snap_dedup(J, map, nmax, ref)) == 0)
    tr = emitir(IRT(IR_PVAL, irt_type(ir->t)), ref - REF_BIAS, 0);
  return tr;
}

/* Check whether a sunk store corresponds to an allocation. Slow path. */
static int snap_sunk_store2(GCtrace *T, IRIns *ira, IRIns *irs)
{
  if (irs->o == IR_ASTORE || irs->o == IR_HSTORE ||
      irs->o == IR_FSTORE || irs->o == IR_XSTORE) {
    IRIns *irk = &T->ir[irs->op1];
    if (irk->o == IR_AREF || irk->o == IR_HREFK)
      irk = &T->ir[irk->op1];
    return (&T->ir[irk->op1] == ira);
  }
  return 0;
}

/* Check whether a sunk store corresponds to an allocation. Fast path. */
static LJ_AINLINE int snap_sunk_store(GCtrace *T, IRIns *ira, IRIns *irs)
{
  if (!isfarsunkstore(irs->s)) {
    return (ira + irs->s == irs);  /* Fast check. */
  }
  return snap_sunk_store2(T, ira, irs);
}

/* Replay snapshot state to setup side trace. */
void lj_snap_replay(jit_State *J, GCtrace *T)
{
  SnapShot *snap = &T->snap[J->exitno];
  SnapEntry *map = &T->snapmap[snap->mapofs];
  size_t n, nent = snap->nent;
  BloomFilter seen = 0;
  int pass23 = 0;
  J->framedepth = 0;
  /* Emit IR for slots inherited from parent snapshot. */
  for (n = 0; n < nent; n++) {
    SnapEntry sn = map[n];
    BCReg s = snap_slot(sn);
    IRRef ref = snap_ref(sn);
    IRIns *ir = &T->ir[ref];
    TRef tr;
    /* The bloom filter avoids O(nent^2) overhead for de-duping slots. */
    if (bloomtest(seen, ref) && (tr = snap_dedup(J, map, n, ref)) != 0)
      goto setslot;
    bloomset(seen, ref);
    if (irref_isk(ref)) {
      tr = snap_replay_const(J, ir);
    } else if (!regsp_used(ir->prev)) {
      pass23 = 1;
      lua_assert(s != 0);
      tr = s;
    } else {
      IRType t = irt_type(ir->t);
      uint32_t mode = IRSLOAD_INHERIT|IRSLOAD_PARENT;
      if (ir->o == IR_SLOAD) mode |= (ir->op2 & IRSLOAD_READONLY);
      tr = emitir_raw(IRT(IR_SLOAD, t), s, mode);
    }
  setslot:
    J->slot[s] = tr | (sn&(SNAP_TREF_FLAGMASK));  /* Same as TREF_* flags. */
    J->framedepth += ((sn & (SNAP_CONT|SNAP_FRAME)) && s);
    if ((sn & SNAP_FRAME))
      J->baseslot = s+1;
  }
  if (pass23) {
    IRIns *irlast = &T->ir[snap->ref];
    pass23 = 0;
    /* Emit dependent PVALs. */
    for (n = 0; n < nent; n++) {
      SnapEntry sn = map[n];
      IRRef refp = snap_ref(sn);
      IRIns *ir = &T->ir[refp];
      if (regsp_reg(ir->r) == RID_SUNK) {
        if (J->slot[snap_slot(sn)] != snap_slot(sn)) continue;
        pass23 = 1;
        lua_assert(ir->o == IR_TNEW || irt_ktdup(T, ir) ||
                   ir->o == IR_CNEW || ir->o == IR_CNEWI);
        if (ir->op1 >= T->nk) snap_pref(J, T, map, nent, seen, ir->op1);
        if (ir->op2 >= T->nk) snap_pref(J, T, map, nent, seen, ir->op2);
        if (LJ_HASFFI && ir->o == IR_CNEWI) {
        } else {
          IRIns *irs;
          for (irs = ir+1; irs < irlast; irs++)
            if (irs->r == RID_SINK && snap_sunk_store(T, ir, irs)) {
              if (snap_pref(J, T, map, nent, seen, irs->op2) == 0)
                snap_pref(J, T, map, nent, seen, T->ir[irs->op2].op1);
            }
        }
      } else if (!irref_isk(refp) && !regsp_used(ir->prev)) {
        lua_assert(ir->o == IR_CONV && ir->op2 == IRCONV_NUM_INT);
        J->slot[snap_slot(sn)] = snap_pref(J, T, map, nent, seen, ir->op1);
      }
    }
    /* Replay sunk instructions. */
    for (n = 0; pass23 && n < nent; n++) {
      SnapEntry sn = map[n];
      IRRef refp = snap_ref(sn);
      IRIns *ir = &T->ir[refp];
      if (regsp_reg(ir->r) == RID_SUNK) {
        TRef op1, op2;
        if (J->slot[snap_slot(sn)] != snap_slot(sn)) {  /* De-dup allocs. */
          J->slot[snap_slot(sn)] = J->slot[J->slot[snap_slot(sn)]];
          continue;
        }
        op1 = ir->op1;
        if (op1 >= T->nk) op1 = snap_pref(J, T, map, nent, seen, op1);
        op2 = ir->op2;
        if (op2 >= T->nk) op2 = snap_pref(J, T, map, nent, seen, op2);
        if (LJ_HASFFI && ir->o == IR_CNEWI) {
          J->slot[snap_slot(sn)] = emitir(ir->ot & ~(IRT_MARK|IRT_ISPHI), op1, op2);
        } else {
          IRIns *irs;
          TRef tr = emitir(ir->ot, op1, op2);
          J->slot[snap_slot(sn)] = tr;
          for (irs = ir+1; irs < irlast; irs++)
            if (irs->r == RID_SINK && snap_sunk_store(T, ir, irs)) {
              IRIns *irr = &T->ir[irs->op1];
              TRef val, key = irr->op2, tmp = tr;
              if (irr->o != IR_FREF) {
                IRIns *irk = &T->ir[key];
                if (irr->o == IR_HREFK)
                  key = lj_ir_kslot(J, snap_replay_const(J, &T->ir[irk->op1]),
                                    irk->op2);
                else
                  key = snap_replay_const(J, irk);
                if (irr->o == IR_HREFK || irr->o == IR_AREF) {
                  IRIns *irf = &T->ir[irr->op1];
                  tmp = emitir(irf->ot, tmp, irf->op2);
                }
              }
              tmp = emitir(irr->ot, tmp, key);
              val = snap_pref(J, T, map, nent, seen, irs->op2);
              if (val == 0) {
                IRIns *irc = &T->ir[irs->op2];
                lua_assert(irc->o == IR_CONV && irc->op2 == IRCONV_NUM_INT);
                val = snap_pref(J, T, map, nent, seen, irc->op1);
                val = emitir(IRTN(IR_CONV), val, IRCONV_NUM_INT);
              }
              emitir(irs->ot, tmp, val);
            } else if (LJ_HASFFI && irs->o == IR_XBAR && ir->o == IR_CNEW) {
              emitir(IRT(IR_XBAR, IRT_NIL), 0, 0);
            }
        }
      }
    }
  }
  J->base = J->slot + J->baseslot;
  J->maxslot = snap->nslots - J->baseslot;
  lj_snap_add(J);
  if (pass23)  /* Need explicit GC step _after_ initial snapshot. */
    emitir_raw(IRTG(IR_GCSTEP, IRT_NIL), 0, 0);
}

/* -- Snapshot restore ---------------------------------------------------- */

static void snap_unsink(jit_State *J, GCtrace *T, ExitState *ex,
                        SnapNo snapno, BloomFilter rfilt,
                        IRIns *ir, TValue *o);

/* Restore a value from the trace exit state. */
static void snap_restoreval(jit_State *J, GCtrace *T, ExitState *ex,
                            SnapNo snapno, BloomFilter rfilt,
                            IRRef ref, TValue *o, int low)
{
  IRIns *ir = &T->ir[ref];
  IRType1 t = ir->t;
  RegSP rs = ir->prev;
  if (irref_isk(ref)) {  /* Restore constant slot. */
    lj_ir_kvalue(J->L, o, ir);
    return;
  }
  if (LJ_UNLIKELY(bloomtest(rfilt, ref)))
    rs = snap_renameref(T, snapno, ref, rs);
  if (ra_hasspill(regsp_spill(rs))) {  /* Restore from spill slot. */
    int32_t *sps = &ex->spill[regsp_spill(rs)];
    if (low) {
      o->u32.lo = *(uint32_t *)sps;
    } else if (irt_isinteger(t)) {
      setintV(o, *sps);
    } else if (irt_isnum(t)) {
      setrawV(o, *(uint64_t *)sps);
    } else if (irt_islightud(t)) {
      /* 64 bit lightuserdata which may escape already has the tag bits. */
      o->u64 = *(uint64_t *)sps; /* TODO: this is to be fixed when lightud is refactored. */
    } else {
      if (!irt_ispri(t)) {
        setgcV(J->L, o, *(GCobj **)(void *)sps, irt_toitype(t));
      } else {
        /* PRI refs can have a spill slot in case of mispredicted MOVTVPRI. */
        lua_assert((J->flags & JIT_F_OPT_MOVTVPRI));
        settag(o, irt_toitype(t));
      }
    }
  } else {  /* Restore from register. */
    Reg r = regsp_reg(rs);
    lua_assert(r != RID_INTERNAL); /* Never allocated. */
    if (ra_noreg(r)) {
      lua_assert(ir->o == IR_CONV && ir->op2 == IRCONV_NUM_INT);
      snap_restoreval(J, T, ex, snapno, rfilt, ir->op1, o, low);
      return;
    } else if (low) {
      o->u32.lo = (uint32_t)ex->gpr[r-RID_MIN_GPR];
    } else if (irt_isinteger(t)) {
      setintV(o, (int32_t)ex->gpr[r-RID_MIN_GPR]);
    } else if (irt_isnum(t)) {
      setnumV(o, ex->fpr[r-RID_MIN_FPR]);
    } else if (irt_islightud(t)) {
      /* 64 bit lightuserdata which may escape already has the tag bits. */
      o->u64 = ex->gpr[r-RID_MIN_GPR]; /* TODO: this is to be fixed when lightud is refactored. */
    } else {
      if (!irt_ispri(t)) {
        setgcV(J->L, o, (GCobj*)(uintptr_t)(ex->gpr[r-RID_MIN_GPR]), irt_toitype(t));
      } else {
        settag(o, irt_toitype(t));
      }
    }
  }
}

#if LJ_HASFFI
/* Restore raw data from the trace exit state. */
static void snap_restoredata(GCtrace *T, ExitState *ex,
                             SnapNo snapno, BloomFilter rfilt,
                             IRRef ref, void *dst, CTSize sz)
{
  IRIns *ir = &T->ir[ref];
  RegSP rs = ir->prev;
  int32_t *src;
  uint64_t tmp;
  if (irref_isk(ref)) {
    if (ir->o == IR_KNUM || ir->o == IR_KINT64) {
      src = (int32_t *)ir->ptr;
    } else {
      src = (int32_t *)&ir->i;
    }
  } else {
    if (LJ_UNLIKELY(bloomtest(rfilt, ref)))
      rs = snap_renameref(T, snapno, ref, rs);
    if (ra_hasspill(regsp_spill(rs))) {
      src = &ex->spill[regsp_spill(rs)];
      if (sz == 8 && !irt_is64(ir->t)) {
        tmp = (uint64_t)(uint32_t)*src;
        src = (int32_t *)&tmp;
      }
    } else {
      Reg r = regsp_reg(rs);
      if (ra_noreg(r)) {
        lua_assert(sz == 8 && ir->o == IR_CONV && ir->op2 == IRCONV_NUM_INT);
        snap_restoredata(T, ex, snapno, rfilt, ir->op1, dst, 4);
        *(lua_Number *)dst = (lua_Number)*(int32_t *)dst;
        return;
      }
      src = (int32_t *)&ex->gpr[r-RID_MIN_GPR];
      if (r >= RID_MAX_GPR) {
        src = (int32_t *)&ex->fpr[r-RID_MIN_FPR];
      }
    }
  }
  lua_assert(sz == 1 || sz == 2 || sz == 4 || sz == 8);
  if (sz == 4) *(int32_t *)dst = *src;
  else if (sz == 8) *(int64_t *)dst = *(int64_t *)src;
  else if (sz == 1) *(int8_t *)dst = (int8_t)*src;
  else *(int16_t *)dst = (int16_t)*src;
}
#endif

/* Unsink allocation from the trace exit state. Unsink sunk stores. */
static void snap_unsink(jit_State *J, GCtrace *T, ExitState *ex,
                        SnapNo snapno, BloomFilter rfilt,
                        IRIns *ir, TValue *o)
{
  lua_assert(ir->o == IR_TNEW || irt_ktdup(T, ir) ||
             ir->o == IR_CNEW || ir->o == IR_CNEWI);
#if LJ_HASFFI
  if (ir->o == IR_CNEW || ir->o == IR_CNEWI) {
    CTState *cts = ctype_cts(J->L);
    CTypeID id = (CTypeID)T->ir[ir->op1].i;
    CTSize sz = lj_ctype_size(cts, id);
    GCcdata *cd = lj_cdata_new(cts, id, sz);
    setcdataV(J->L, o, cd);
    if (ir->o == IR_CNEWI) {
      uint8_t *p = (uint8_t *)cdataptr(cd);
      lua_assert(sz == 4 || sz == 8);
      snap_restoredata(T, ex, snapno, rfilt, ir->op2, p, sz);
    } else {
      IRIns *irs, *irlast = &T->ir[T->snap[snapno].ref];
      for (irs = ir+1; irs < irlast; irs++)
        if (irs->r == RID_SINK && snap_sunk_store(T, ir, irs)) {
          IRIns *iro = &T->ir[T->ir[irs->op1].op2];
          uint8_t *p = (uint8_t *)cd;
          CTSize szs;
          lua_assert(irs->o == IR_XSTORE && T->ir[irs->op1].o == IR_ADD);
          lua_assert(iro->o == IR_KINT || iro->o == IR_KINT64);
          if (irt_is64(irs->t)) szs = 8;
          else if (irt_isi8(irs->t) || irt_isu8(irs->t)) szs = 1;
          else if (irt_isi16(irs->t) || irt_isu16(irs->t)) szs = 2;
          else szs = 4;
          if (iro->o == IR_KINT64)
            p += (int64_t)rawV(ir_k64(iro));
          else
            p += iro->i;
          lua_assert(p >= (uint8_t *)cdataptr(cd) &&
                     p + szs <= (uint8_t *)cdataptr(cd) + sz);
          snap_restoredata(T, ex, snapno, rfilt, irs->op2, p, szs);
        }
    }
  } else
#endif
  {
    IRIns *irs, *irlast;
    GCtab *t = ir->o == IR_TNEW ? lj_tab_new(J->L, ir->op1, ir->op2) :
                                  lj_tab_dup(J->L, ir_ktab(&T->ir[ir->op1]));
    settabV(J->L, o, t);
    irlast = &T->ir[T->snap[snapno].ref];
    for (irs = ir+1; irs < irlast; irs++)
      if (irs->r == RID_SINK && snap_sunk_store(T, ir, irs)) {
        IRIns *irk = &T->ir[irs->op1];
        TValue tmp, *val;
        lua_assert(irs->o == IR_ASTORE || irs->o == IR_HSTORE ||
                   irs->o == IR_FSTORE);
        if (irk->o == IR_FREF) {
          lua_assert(irk->op2 == IRFL_TAB_META);
          snap_restoreval(J, T, ex, snapno, rfilt, irs->op2, &tmp, 0);
          /* NOBARRIER: The table is new (marked white). */
          t->metatable = tabV(&tmp);
        } else {
          irk = &T->ir[irk->op2];
          if (irk->o == IR_KSLOT) irk = &T->ir[irk->op1];
          lj_ir_kvalue(J->L, &tmp, irk);
          val = lj_tab_set(J->L, t, &tmp);
          /* NOBARRIER: The table is new (marked white). */
          snap_restoreval(J, T, ex, snapno, rfilt, irs->op2, val, 0);
        }
      }
  }
}

/* Restore interpreter state from exit state with the help of a snapshot. */
BCIns *lj_snap_restore(jit_State *J, void *exptr)
{
  ExitState *ex = (ExitState *)exptr;
  SnapNo snapno = J->exitno;  /* For now, snapno == exitno. */
  GCtrace *T = traceref(J, J->parent);
  SnapShot *snap = &T->snap[snapno];
  size_t n, nent = snap->nent;
  SnapEntry *map = &T->snapmap[snap->mapofs];
  SnapEntry *flinks = &T->snapmap[snap_nextofs(T, snap)-2];
  int64_t ftsz0;
  TValue *frame;
  BloomFilter rfilt = snap_renamefilter(T, snapno);
  BCIns *pc = snap_pc(&map[nent]);
  lua_State *L = J->L;

  /* Set interpreter PC to the next PC to get correct error messages. */
  uj_cframe_pc_set(uj_cframe_raw(L->cframe), pc+1);

  /* Make sure the stack is big enough for the slots from the snapshot. */
  if (LJ_UNLIKELY(L->base + snap->topslot >= L->maxstack)) {
    L->top = curr_topL(L);
    uj_state_stack_grow(L, snap->topslot - curr_proto(L)->framesize);
  }

  /* Fill stack slots with data from the registers and spill slots. */
  frame = L->base-1;
  ftsz0 = frame_ftsz(frame);  /* Preserve link to previous frame in slot #0. */
  for (n = 0; n < nent; n++) {
    SnapEntry sn = map[n];
    if (!(sn & SNAP_NORESTORE)) {
      TValue *o = &frame[snap_slot(sn)];
      IRRef ref = snap_ref(sn);
      IRIns *ir = &T->ir[ref];
      if (ir->r == RID_SUNK) {
        size_t j;
        for (j = 0; j < n; j++)
          if (snap_ref(map[j]) == ref) {  /* De-duplicate sunk allocations. */
            copyTV(L, o, &frame[snap_slot(map[j])]);
            goto dupslot;
          }
        snap_unsink(J, T, ex, snapno, rfilt, ir, o);
      dupslot:
        continue;
      }
      snap_restoreval(J, T, ex, snapno, rfilt, ref, o, sn & SNAP_LOW);
      if ((sn & (SNAP_CONT|SNAP_FRAME))) {
        /* Overwrite tag with frame link. */
        if (snap_slot(sn) != 0) {
          o->fr.tp.ftsz = *(uint64_t *)flinks; flinks -= 2;
        } else {
          o->fr.tp.ftsz = ftsz0;
        }
        L->base = o+1;
      }
    }
  }
  lua_assert(map + nent == flinks);

  /* Compute current stack top. */
  switch (bc_op(*pc)) {
  default:
    if (bc_op(*pc) < BC_FUNCF) {
      L->top = curr_topL(L);
      break;
    }
    /* fallthrough */
  case BC_CALLM: case BC_CALLMT: case BC_RETM: case BC_TSETM:
    L->top = frame + snap->nslots;
    break;
  }

  J->nsnaprestore++;

  return pc;
}

#undef emitir_raw
#undef emitir

#endif
