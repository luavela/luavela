/*
 * Lua parser (source code -> bytecode).
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _LJ_PARSE_H
#define _LJ_PARSE_H

#include "lj_obj.h"
#include "frontend/lj_lex.h"

/* Index into variable stack. */
typedef uint16_t VarIndex;

/* Per-function linked list of scope blocks. */
typedef struct FuncScope {
  struct FuncScope *prev;       /* Link to outer scope. */
  size_t vstart;                /* Start of block-local variables. */
  uint8_t nactvar;              /* Number of active vars outside the scope. */
  uint8_t flags;                /* Scope flags. */
} FuncScope;

/* Per-function state. */
typedef struct FuncState {
  GCtab *kt;                    /* Hash table for constants. */
  LexState *ls;                 /* Lexer state. */
  lua_State *L;                 /* Lua state. */
  FuncScope *bl;                /* Current scope. */
  struct FuncState *prev;       /* Enclosing function. */
  BCPos pc;                     /* Next bytecode position. */
  BCPos lasttarget;             /* Bytecode position of last jump target. */
  BCPos jpc;                    /* Pending jump list to next bytecode. */
  BCReg freereg;                /* First free register. */
  BCReg nactvar;                /* Number of active local variables. */
  BCReg nkn, nkgc;              /* Number of lua_Number/GCobj constants */
  BCLine linedefined;           /* First line of the function definition. */
  BCInsLine *bcbase;            /* Base of bytecode stack. */
  BCPos bclim;                  /* Limit of bytecode stack. */
  size_t vbase;                 /* Base of variable stack for this function. */
  uint8_t flags;                /* Prototype flags. */
  uint8_t numparams;            /* Number of parameters. */
  uint8_t framesize;            /* Fixed frame size. */
  uint8_t nuv;                  /* Number of upvalues */
  VarIndex varmap[LJ_MAX_LOCVAR];  /* Map from register to variable idx. */
  VarIndex uvmap[LJ_MAX_UPVAL]; /* Map from upvalue to variable idx. */
  VarIndex uvtmp[LJ_MAX_UPVAL]; /* Temporary upvalue map. */
} FuncState;

GCproto *lj_parse(LexState *ls);
BCPos lj_parse_bcemit(FuncState *fs, BCIns ins);
GCstr *lj_parse_keepstr(LexState *ls, const char *str, size_t l);
#if LJ_HASFFI
void lj_parse_keepcdata(LexState *ls, TValue *tv, GCcdata *cd);
#endif

#endif
