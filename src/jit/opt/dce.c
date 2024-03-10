/*
 * DCE: Dead Code Elimination. Pre-LOOP only -- ASM already performs DCE.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "lj_obj.h"

#if LJ_HASJIT

#include "jit/lj_ir.h"
#include "jit/lj_jit.h"
#include "jit/lj_iropt.h"

/* Some local macros to save typing. Undef'd at the end. */
#define IR(ref)         (&J->cur.ir[(ref)])

/* Scan through all snapshots and mark all referenced instructions. */
static void dce_marksnap(jit_State *J)
{
  SnapNo i, nsnap = J->cur.nsnap;
  for (i = 0; i < nsnap; i++) {
    SnapShot *snap = &J->cur.snap[i];
    SnapEntry *map = &J->cur.snapmap[snap->mapofs];
    size_t n, nent = snap->nent;
    for (n = 0; n < nent; n++) {
      IRRef ref = snap_ref(map[n]);
      if (ref >= REF_FIRST)
        irt_setmark(IR(ref)->t);
    }
  }
}

/* Backwards propagate marks. Replace unused instructions with NOPs. */
static void dce_propagate(jit_State *J)
{
  IRRef1 *pchain[IR__MAX];
  IRRef ins;
  uint32_t i;
  for (i = 0; i < IR__MAX; i++) pchain[i] = &J->chain[i];
  for (ins = J->cur.nins-1; ins >= REF_FIRST; ins--) {
    IRIns *ir = IR(ins);
    if (irt_ismarked(ir->t)) {
      irt_clearmark(ir->t);
      pchain[ir->o] = &ir->prev;
    } else if (!ir_sideeff(ir)) {
      *pchain[ir->o] = ir->prev;  /* Reroute original instruction chain. */
      ir_tonop(ir);
      continue;
    }
    if (ir->op1 >= REF_FIRST) irt_setmark(IR(ir->op1)->t);
    if (ir->op2 >= REF_FIRST) irt_setmark(IR(ir->op2)->t);
  }
}

/* Dead Code Elimination.
**
** First backpropagate marks for all used instructions. Then replace
** the unused ones with a NOP. Note that compressing the IR to eliminate
** the NOPs does not pay off.
*/
void lj_opt_dce(jit_State *J)
{
  if ((J->flags & JIT_F_OPT_DCE)) {
    dce_marksnap(J);
    dce_propagate(J);
    memset(J->bpropcache, 0, sizeof(J->bpropcache));  /* Invalidate cache. */
  }
}

#undef IR

#endif
