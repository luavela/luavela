/*
 * Instruction dispatch handling.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _UJ_DISPATCH_H
#define _UJ_DISPATCH_H

#include "lj_obj.h"
#include "lj_bc.h"
#if LJ_HASJIT
#include "jit/lj_jit.h"
#endif /* LJ_HASJIT */
#include "lj_vm.h"

#define HOTCOUNT_STEP 1

/* This solves a circular dependency problem -- bump as needed. Sigh. */
#define GG_NUM_ASMFF 62

#define GG_LEN_DDISP (BC__MAX + GG_NUM_ASMFF)
#define GG_LEN_SDISP BC_FUNCF
#define GG_LEN_DISP (GG_LEN_DDISP + GG_LEN_SDISP)

/* Global state, main thread and extra fields are allocated together. */
struct GG_State {
	struct lua_State L; /* Main thread */
	struct global_State g; /* Global state */
#if LJ_HASJIT
	struct jit_State J; /* JIT state */
#endif /* LJ_HASJIT */
	ASMFunction dispatch[GG_LEN_DISP]; /* Instruction dispatch tables */
	BCIns bcff[GG_NUM_ASMFF]; /* Bytecode for ASM fast functions */
};

#define GG_OFS(field) ((int)offsetof(struct GG_State, field))
#define G2GG(gl) ((struct GG_State *)((char *)(gl)-GG_OFS(g)))
#define J2GG(j) ((struct GG_State *)((char *)(j)-GG_OFS(J)))
#define L2GG(L) (G2GG(G(L)))
#define J2G(J) (&J2GG(J)->g)
#define G2J(gl) (&G2GG(gl)->J)
#define L2J(L) (&L2GG(L)->J)
#define GG_G2DISP (GG_OFS(dispatch) - GG_OFS(g))
#define GG_DISP2G (GG_OFS(g) - GG_OFS(dispatch))
#define GG_DISP2J (GG_OFS(J) - GG_OFS(dispatch))
#define GG_DISP2HOT (GG_OFS(hotcount) - GG_OFS(dispatch))
#define GG_DISP2STATIC (GG_LEN_DDISP * (int)sizeof(ASMFunction))

#if LJ_HASJIT
/* Exchanging these fields blows up the compiler: */
LJ_STATIC_ASSERT(GG_OFS(dispatch) > GG_OFS(J));
#endif

/* Dispatch table management. */
void uj_dispatch_init(struct GG_State *GG);
#if LJ_HASJIT
void uj_dispatch_init_hotcount(struct global_State *g);
#endif /* LJ_HASJIT */
void uj_dispatch_update(struct global_State *g);

#endif /* !_UJ_DISPATCH_H */
