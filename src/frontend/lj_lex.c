/*
 * Lexical analyzer.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 *
 * Major portions taken verbatim or adapted from the Lua interpreter.
 * Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
 */

#include "lj_obj.h"
#include "lj_gc.h"
#include "uj_mem.h"
#include "uj_throw.h"
#include "uj_errmsg.h"
#include "uj_proto.h"
#include "uj_str.h"
#include "uj_sbuf.h"
#if LJ_HASFFI
#include "lj_tab.h"
#include "ffi/lj_ctype.h"
#include "ffi/lj_cdata.h"
#include "lextlib.h"
#endif
#include "uj_state.h"
#include "frontend/lj_lex.h"
#include "frontend/lj_parse.h"
#include "utils/strscan.h"
#include "utils/lj_char.h"

/* Lua lexer token names. */
static const char *const tokennames[] = {
#define TKSTR1(name)            #name,
#define TKSTR2(name, sym)       #sym,
TKDEF(TKSTR1, TKSTR2)
#undef TKSTR1
#undef TKSTR2
  NULL
};

/* -- Buffer handling ----------------------------------------------------- */

#define char2int(c)      ((int)(uint8_t)(c))
#define next(ls) \
  ((ls)->current = ((ls)->n--) > 0 ? char2int(*((ls)->p)++) : fillbuf(ls))
#define save_and_next(ls)       (save((ls), (ls)->current), next(ls))
#define currIsNewline(ls)       ((ls)->current == '\n' || (ls)->current == '\r')
#define END_OF_STREAM           (-1)

static int fillbuf(LexState *ls)
{
  size_t sz;
  const char *buf = ls->rfunc(ls->L, ls->rdata, &sz);
  if (buf == NULL || sz == 0) return END_OF_STREAM;
  ls->n = sz - 1;
  ls->p = buf;
  return char2int(*(ls->p++));
}

static LJ_AINLINE void save(LexState *ls, int c) {
  struct sbuf *sb = &ls->sb;
  uj_sbuf_push_char(sb, (char)c);
  if (uj_sbuf_size(sb) >= LJ_MAX_STR) {
    lj_lex_error(ls, 0, UJ_ERR_XELEM, NULL);
  }
}

static void inclinenumber(LexState *ls)
{
  int old = ls->current;
  lua_assert(currIsNewline(ls));
  next(ls);  /* skip `\n' or `\r' */
  if (currIsNewline(ls) && ls->current != old)
    next(ls);  /* skip `\n\r' or `\r\n' */
  if (++ls->linenumber >= LJ_MAX_LINE)
    lj_lex_error(ls, ls->token, UJ_ERR_XLINES, NULL);
}

/* -- Scanner for terminals ----------------------------------------------- */

/* Parse a number literal. */
static void lex_number(LexState *ls, TValue *tv)
{
  StrScanFmt fmt;
  int c, xp = 'e';
  lua_assert(lj_char_isdigit(ls->current));
  if ((c = ls->current) == '0') {
    save_and_next(ls);
    if (lj_char_casecmp(ls->current, 'x')) xp = 'p';
  }
  while (lj_char_isident(ls->current) || ls->current == '.' ||
         ((ls->current == '-' || ls->current == '+') && lj_char_casecmp(c, xp))) {
    c = ls->current;
    save_and_next(ls);
  }
  save(ls, '\0');
#if LJ_HASFFI
  double tmp;
  fmt = strscan_tonumber((const uint8_t *)uj_sbuf_front(&ls->sb), &tmp,
    STRSCAN_OPT_TONUM | STRSCAN_OPT_LL | STRSCAN_OPT_IMAG);
  if (fmt != STRSCAN_ERROR) {
    setnumV(tv, tmp);
    if (fmt != STRSCAN_NUM) {
      lua_State *L = ls->L;
      GCcdata *cd;
      lua_assert(fmt == STRSCAN_I64 || fmt == STRSCAN_U64 || fmt == STRSCAN_IMAG);
      if (!ctype_ctsG(G(L))) {
        ptrdiff_t oldtop = uj_state_stack_save(L, L->top);
        luaopen_ffi(L);  /* Load FFI library on-demand. */
        L->top = uj_state_stack_restore(L, oldtop);
      }
      if (fmt == STRSCAN_IMAG) {
        cd = lj_cdata_new_(L, CTID_COMPLEX_DOUBLE, 2*sizeof(double));
        ((double *)cdataptr(cd))[0] = 0;
        ((double *)cdataptr(cd))[1] = numV(tv);
      } else {
        cd = lj_cdata_new_(L, fmt==STRSCAN_I64 ? CTID_INT64 : CTID_UINT64, 8);
        *(uint64_t *)cdataptr(cd) = rawV(tv);
      }
      lj_parse_keepcdata(ls, tv, cd);
    }
  } else {
    lua_assert(fmt == STRSCAN_ERROR);
    lj_lex_error(ls, TK_number, UJ_ERR_XNUMBER, NULL);
  }
#else
  if (!uj_cstr_tonumtv(uj_sbuf_front(&ls->sb), tv)) {
    lj_lex_error(ls, TK_number, UJ_ERR_XNUMBER, NULL);
  }
  UNUSED(fmt);
#endif
}

static int skip_sep(LexState *ls)
{
  int count = 0;
  int s = ls->current;
  lua_assert(s == '[' || s == ']');
  save_and_next(ls);
  while (ls->current == '=') {
    save_and_next(ls);
    count++;
  }
  return (ls->current == s) ? count : (-count) - 1;
}

static void read_long_string(LexState *ls, TValue *tv, int sep)
{
  struct sbuf *sb = &ls->sb;
  save_and_next(ls);  /* skip 2nd `[' */
  if (currIsNewline(ls)) { /* string starts with a newline? */
    inclinenumber(ls);  /* skip it */
  }
  for (;;) {
    switch (ls->current) {
    case END_OF_STREAM:
      lj_lex_error(ls, TK_eof, tv ? UJ_ERR_XLSTR : UJ_ERR_XLCOM, NULL);
      break;
    case ']':
      if (skip_sep(ls) == sep) {
        save_and_next(ls);  /* skip 2nd `]' */
        goto endloop;
      }
      break;
    case '\n':
    case '\r':
      save(ls, '\n');
      inclinenumber(ls);
      if (!tv) { uj_sbuf_reset(sb); } /* avoid wasting space */
      break;
    default:
      if (tv) { save_and_next(ls); }
      else next(ls);
      break;
    }
  } endloop:
  if (tv) {
    GCstr *str = lj_parse_keepstr(ls, uj_sbuf_front(sb) + (2 + (size_t)sep),
                                      uj_sbuf_size(sb) - 2*(2 + (size_t)sep));
    setstrV(ls->L, tv, str);
  }
}

static void read_string(LexState *ls, int delim, TValue *tv)
{
  save_and_next(ls);
  while (ls->current != delim) {
    switch (ls->current) {
    case END_OF_STREAM:
      lj_lex_error(ls, TK_eof, UJ_ERR_XSTR, NULL);
      continue;
    case '\n':
    case '\r':
      lj_lex_error(ls, TK_string, UJ_ERR_XSTR, NULL);
      continue;
    case '\\': {
      int c = next(ls);  /* Skip the '\\'. */
      switch (c) {
      case 'a': c = '\a'; break;
      case 'b': c = '\b'; break;
      case 'f': c = '\f'; break;
      case 'n': c = '\n'; break;
      case 'r': c = '\r'; break;
      case 't': c = '\t'; break;
      case 'v': c = '\v'; break;
      case 'x':  /* Hexadecimal escape '\xXX'. */
        c = (next(ls) & 15u) << 4;
        if (!lj_char_isdigit(ls->current)) {
          if (!lj_char_isxdigit(ls->current)) goto err_xesc;
          c += 9 << 4;
        }
        c += (next(ls) & 15u);
        if (!lj_char_isdigit(ls->current)) {
          if (!lj_char_isxdigit(ls->current)) goto err_xesc;
          c += 9;
        }
        break;
      case 'z':  /* Skip whitespace. */
        next(ls);
        while (lj_char_isspace(ls->current))
          if (currIsNewline(ls)) inclinenumber(ls); else next(ls);
        continue;
      case '\n': case '\r': save(ls, '\n'); inclinenumber(ls); continue;
      case '\\': case '\"': case '\'': break;
      case END_OF_STREAM: continue;
      default:
        if (!lj_char_isdigit(c))
          goto err_xesc;
        c -= '0';  /* Decimal escape '\ddd'. */
        if (lj_char_isdigit(next(ls))) {
          c = c*10 + (ls->current - '0');
          if (lj_char_isdigit(next(ls))) {
            c = c*10 + (ls->current - '0');
            if (c > 255) {
            err_xesc:
              lj_lex_error(ls, TK_string, UJ_ERR_XESC, NULL);
            }
            next(ls);
          }
        }
        save(ls, c);
        continue;
      }
      save(ls, c);
      next(ls);
      continue;
      }
    default:
      save_and_next(ls);
      break;
    }
  }
  save_and_next(ls);  /* skip delimiter */
  setstrV(ls->L, tv, lj_parse_keepstr(ls, uj_sbuf_at(&ls->sb, 1),
                                      uj_sbuf_size(&ls->sb) - 2));
}

/* -- Main lexical scanner ------------------------------------------------ */

static int llex(LexState *ls, TValue *tv)
{
  struct sbuf *sb = &ls->sb;
  uj_sbuf_reset(sb);
  for (;;) {
    if (lj_char_isident(ls->current)) {
      GCstr *s;
      if (lj_char_isdigit(ls->current)) {  /* Numeric literal. */
        lex_number(ls, tv);
        return TK_number;
      }
      /* Identifier or reserved word. */
      do {
        save_and_next(ls);
      } while (lj_char_isident(ls->current));
      s = lj_parse_keepstr(ls, uj_sbuf_front(sb), uj_sbuf_size(sb));
      setstrV(ls->L, tv, s);
      if (s->reserved > 0)  /* Reserved word? */
        return TK_OFS + s->reserved;
      return TK_name;
    }
    switch (ls->current) {
    case '\n':
    case '\r':
      inclinenumber(ls);
      continue;
    case ' ':
    case '\t':
    case '\v':
    case '\f':
      next(ls);
      continue;
    case '-':
      next(ls);
      if (ls->current != '-') return '-';
      /* else is a comment */
      next(ls);
      if (ls->current == '[') {
        int sep = skip_sep(ls);
        uj_sbuf_reset(sb);  /* `skip_sep' may dirty the buffer */
        if (sep >= 0) {
          read_long_string(ls, NULL, sep);  /* long comment */
          uj_sbuf_reset(sb);
          continue;
        }
      }
      /* else short comment */
      while (!currIsNewline(ls) && ls->current != END_OF_STREAM)
        next(ls);
      continue;
    case '[': {
      int sep = skip_sep(ls);
      if (sep >= 0) {
        read_long_string(ls, tv, sep);
        return TK_string;
      } else if (sep == -1) {
        return '[';
      } else {
        lj_lex_error(ls, TK_string, UJ_ERR_XLDELIM, NULL);
        continue;
      }
      }
    case '=':
      next(ls);
      if (ls->current != '=') return '='; else { next(ls); return TK_eq; }
    case '<':
      next(ls);
      if (ls->current != '=') return '<'; else { next(ls); return TK_le; }
    case '>':
      next(ls);
      if (ls->current != '=') return '>'; else { next(ls); return TK_ge; }
    case '~':
      next(ls);
      if (ls->current != '=') return '~'; else { next(ls); return TK_ne; }
    case ':':
      next(ls);
      if (ls->current != ':') return ':'; else { next(ls); return TK_label; }
    case '"':
    case '\'':
      read_string(ls, ls->current, tv);
      return TK_string;
    case '.':
      save_and_next(ls);
      if (ls->current == '.') {
        next(ls);
        if (ls->current == '.') {
          next(ls);
          return TK_dots;   /* ... */
        }
        return TK_concat;   /* .. */
      } else if (!lj_char_isdigit(ls->current)) {
        return '.';
      } else {
        lex_number(ls, tv);
        return TK_number;
      }
    case END_OF_STREAM:
      return TK_eof;
    default: {
      int c = ls->current;
      next(ls);
      return c;  /* Single-char tokens (+ - / ...). */
    }
    }
  }
}

/* -- Lexer API ----------------------------------------------------------- */

/* Setup lexer state. */
int lj_lex_setup(lua_State *L, LexState *ls)
{
  int header = 0;
  ls->L = L;
  ls->fs = NULL;
  ls->n = 0;
  ls->p = NULL;
  ls->vstack = NULL;
  ls->sizevstack = 0;
  ls->vtop = 0;
  ls->bcstack = NULL;
  ls->sizebcstack = 0;
  ls->token = 0;
  ls->lookahead = TK_eof;  /* No look-ahead token. */
  ls->linenumber = 1;
  ls->lastline = 1;
  uj_sbuf_init(L, &ls->sb);
  next(ls);  /* Read-ahead first char. */
  if (ls->current == 0xef && ls->n >= 2 && char2int(ls->p[0]) == 0xbb &&
      char2int(ls->p[1]) == 0xbf) {  /* Skip UTF-8 BOM (if buffered). */
    ls->n -= 2;
    ls->p += 2;
    next(ls);
    header = 1;
  }
  if (ls->current == '#') {  /* Skip POSIX #! header line. */
    do {
      next(ls);
      if (ls->current == END_OF_STREAM) return 0;
    } while (!currIsNewline(ls));
    inclinenumber(ls);
    header = 1;
  }
  if (ls->current == LUA_SIGNATURE[0]) {  /* Bytecode dump. */
    if (header) {
      /*
      ** Loading bytecode with an extra header is disabled for security
      ** reasons. This may circumvent the usual check for bytecode vs.
      ** Lua code by looking at the first char. Since this is a potential
      ** security violation no attempt is made to echo the chunkname either.
      */
      setstrV(L, L->top++, uj_errmsg_str(L, UJ_ERR_BCBAD));
      uj_throw(L, LUA_ERRSYNTAX);
    }
    return 1;
  }
  return 0;
}

/* Cleanup lexer state. */
void lj_lex_cleanup(lua_State *L, LexState *ls)
{
  uj_mem_free(MEM(L), ls->bcstack, ls->sizebcstack * sizeof(BCInsLine));
  uj_mem_free(MEM(L), ls->vstack, ls->sizevstack * sizeof(VarInfo));
  uj_sbuf_free(L, &ls->sb);
}

void lj_lex_next(LexState *ls)
{
  ls->lastline = ls->linenumber;
  if (LJ_LIKELY(ls->lookahead == TK_eof)) {  /* No lookahead token? */
    ls->token = llex(ls, &ls->tokenval);  /* Get next token. */
  } else {  /* Otherwise return lookahead token. */
    ls->token = ls->lookahead;
    ls->lookahead = TK_eof;
    ls->tokenval = ls->lookaheadval;
  }
}

LexToken lj_lex_lookahead(LexState *ls)
{
  lua_assert(ls->lookahead == TK_eof);
  ls->lookahead = llex(ls, &ls->lookaheadval);
  return ls->lookahead;
}

const char *lj_lex_token2str(LexState *ls, LexToken token)
{
  if (token > TK_OFS)
    return tokennames[token-TK_OFS-1];
  else if (!lj_char_iscntrl(token))
    return uj_str_pushf(ls->L, "%c", token);
  else
    return uj_str_pushf(ls->L, "char(%d)", token);
}

static const char *lex_error_tok_str(LexState *ls, LexToken token)
{
  if (token == 0) {
    return NULL;
  }

  if (TK_name == token  || TK_string == token || TK_number == token) {
    save(ls, '\0');
    return uj_sbuf_front(&ls->sb);
  }

  return lj_lex_token2str(ls, token);
}

LJ_NORET static void lex_error_throw(lua_State *L, const char *msg, const char *tok_str)
{
  if (tok_str != NULL)
    uj_str_pushf(L, uj_errmsg(UJ_ERR_XNEAR), msg, tok_str);

  uj_throw(L, LUA_ERRSYNTAX);
}

void lj_lex_error_(LexState *ls, LexToken token, const char *em, ...)
{
  lua_State *L = ls->L;
  const char *msg_full;
  const char *tok_str;
  char chunkname[LUA_IDSIZE];
  va_list argp;

  tok_str = lex_error_tok_str(ls, token);
  uj_proto_chunknamencpy(chunkname, ls->chunkname, sizeof(chunkname));
  va_start(argp, em);
  msg_full = uj_str_pushvf(L, em, argp);
  msg_full = uj_str_pushf(L, "%s:%d: %s", chunkname, ls->linenumber, msg_full);
  va_end(argp);

  lex_error_throw(L, msg_full, tok_str);
}

void lj_lex_init(lua_State *L)
{
  uint32_t i;
  for (i = 0; i < TK_RESERVED; i++) {
    GCstr *s = uj_str_newz(L, tokennames[i]);
    fixstring(s);  /* Reserved words are never collected. */
    s->reserved = (uint8_t)(i+1);
  }
}

