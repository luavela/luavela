/*
 * Lexical analyzer.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _LJ_LEX_H
#define _LJ_LEX_H

#include <stdarg.h>

#include "lj_obj.h"
#include "uj_errmsg.h"

/* Lua lexer tokens. */
#define TKDEF(_, __) \
  _(and) _(break) _(do) _(else) _(elseif) _(end) _(false) \
  _(for) _(function) _(goto) _(if) _(in) _(local) _(nil) _(not) _(or) \
  _(repeat) _(return) _(then) _(true) _(until) _(while) \
  __(concat, ..) __(dots, ...) __(eq, ==) __(ge, >=) __(le, <=) __(ne, ~=) \
  __(label, ::) __(number, <number>) __(name, <name>) __(string, <string>) \
  __(eof, <eof>)

enum {
  TK_OFS = 256,
#define TKENUM1(name)           TK_##name,
#define TKENUM2(name, sym)      TK_##name,
TKDEF(TKENUM1, TKENUM2)
#undef TKENUM1
#undef TKENUM2
  TK_RESERVED = TK_while - TK_OFS
};

typedef int LexToken;

/* Combined bytecode ins/line. Only used during bytecode generation. */
typedef struct BCInsLine {
  BCIns ins;            /* Bytecode instruction. */
  BCLine line;          /* Line number for this bytecode. */
} BCInsLine;

/* Info for local variables. Only used during bytecode generation. */
typedef struct VarInfo {
  GCstr *name;          /* Local variable name or goto/label name. */
  BCPos startpc;        /* First point where the local variable is active. */
  BCPos endpc;          /* First point where the local variable is dead. */
  uint8_t slot;         /* Variable slot. */
  uint8_t info;         /* Variable/goto/label info. */
} VarInfo;

/* Variable/goto/label info. */
#define VSTACK_VAR_RW           0x01    /* R/W variable. */
#define VSTACK_GOTO             0x02    /* Pending goto. */
#define VSTACK_LABEL            0x04    /* Label. */

#define gola_isgoto(v)          ((v)->info & VSTACK_GOTO)
#define gola_islabel(v)         ((v)->info & VSTACK_LABEL)
#define gola_isgotolabel(v)     ((v)->info & (VSTACK_GOTO|VSTACK_LABEL))


/* Lua lexer state. */
typedef struct LexState {
  struct FuncState *fs; /* Current FuncState. Defined in lj_parse.c. */
  struct lua_State *L;  /* Lua state. */
  TValue tokenval;      /* Current token value. */
  TValue lookaheadval;  /* Lookahead token value. */
  int current;          /* Current character (charint). */
  LexToken token;       /* Current token. */
  LexToken lookahead;   /* Lookahead token. */
  size_t n;             /* Bytes left in input buffer. */
  const char *p;        /* Current position in input buffer. */
  struct sbuf sb;       /* String buffer for tokens. */
  lua_Reader rfunc;     /* Reader callback. */
  void *rdata;          /* Reader callback data. */
  BCLine linenumber;    /* Input line counter. */
  BCLine lastline;      /* Line of last token. */
  GCstr *chunkname;     /* Current chunk name (interned string). */
  const char *chunkarg; /* Chunk name argument. */
  const char *mode;     /* Allow loading bytecode (b) and/or source text (t). */
  VarInfo *vstack;      /* Stack for names and extents of local variables. */
  size_t sizevstack;    /* Size of variable stack. */
  size_t vtop;          /* Top of variable stack. */
  BCInsLine *bcstack;   /* Stack for bytecode instructions/line numbers. */
  size_t sizebcstack;   /* Size of bytecode stack. */
  uint32_t level;       /* Syntactical nesting level. */
} LexState;

int lj_lex_setup(lua_State *L, LexState *ls);
void lj_lex_cleanup(lua_State *L, LexState *ls);
void lj_lex_next(LexState *ls);
LexToken lj_lex_lookahead(LexState *ls);
const char *lj_lex_token2str(LexState *ls, LexToken token);
void lj_lex_init(lua_State *L);

/* Throwing syntax errors. */

/*
 * NB! Using a macro preserves signature consistency across the set of
 * interfaces and works around undefined behaviour issue with the arg of
 * enum type preceding the variadic arguments.
 */
LJ_NORET void lj_lex_error_(LexState *ls, LexToken token, const char *em, ...);
#define lj_lex_error(ls, /* LexToken */ token, /* enum err_msg */ em, ...) \
  lj_lex_error_((ls), (token), uj_errmsg(em), __VA_ARGS__)

#endif
