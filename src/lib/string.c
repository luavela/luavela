/*
 * String library.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 *
 * Major portions taken verbatim or adapted from the Lua interpreter.
 * Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
 */

#include <stdio.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "lj_obj.h"
#include "lj_gc.h"
#include "uj_err.h"
#include "uj_cstr.h"
#include "uj_str.h"
#include "uj_sbuf.h"
#include "lj_tab.h"
#include "uj_meta.h"
#include "uj_mtab.h"
#include "uj_state.h"
#include "uj_ff.h"
#include "lj_bcdump.h"
#include "utils/lj_char.h"
#include "uj_lib.h"

/* ------------------------------------------------------------------------ */

#define LJLIB_MODULE_string

LJLIB_ASM(string_len)           LJLIB_REC(.)
{
  uj_lib_checkstr(L, 1);
  return FFH_RETRY;
}

LJLIB_ASM(string_byte)          LJLIB_REC(.)
{
  GCstr *s = uj_lib_checkstr(L, 1);
  int32_t len = (int32_t)s->len;
  int32_t start = uj_lib_optint(L, 2, 1);
  int32_t stop = uj_lib_optint(L, 3, start);
  int32_t n, i;
  const unsigned char *p;
  if (stop < 0) stop += len+1;
  if (start < 0) start += len+1;
  if (start <= 0) start = 1;
  if (stop > len) stop = len;
  if (start > stop) return FFH_RES(0);  /* Empty interval: return no results. */
  start--;
  n = stop - start;
  if ((uint32_t)n > LUAI_MAXCSTACK)
    uj_err_caller(L, UJ_ERR_STRSLC);
  uj_state_stack_check(L, (size_t)n);
  p = (const unsigned char *)strdata(s) + start;
  for (i = 0; i < n; i++)
    setintV(L->base + i-1, p[i]);
  return FFH_RES(n);
}

LJLIB_ASM(string_char)
{
  int i, nargs = (int)(L->top - L->base);
  struct sbuf *sb = uj_sbuf_reset_tmp(L);
  uj_sbuf_reserve(sb, nargs);
  for (i = 1; i <= nargs; i++) {
    int32_t k = uj_lib_checkint(L, i);
    if (!checku8(k))
      uj_err_arg(L, UJ_ERR_BADVAL, i);
    uj_sbuf_push_char(sb, k);
  }
  setstrV(L, L->base-1, uj_str_frombuf(L, sb));
  return FFH_RES(1);
}

LJLIB_ASM(string_sub)           LJLIB_REC(.)
{
  uj_lib_checkstr(L, 1);
  uj_lib_checkint(L, 2);
  setintV(L->base+2, uj_lib_optint(L, 3, -1));
  return FFH_RETRY;
}

LJLIB_ASM(string_rep)
{
  GCstr *s = uj_lib_checkstr(L, 1);
  int32_t k = uj_lib_checkint(L, 2);
  GCstr *sep = uj_lib_optstr(L, 3);
  size_t tlen, len;
  struct sbuf *sb;
  if (k <= 0) {
  empty:
    setstrV(L, L->base-1, G(L)->strempty);
    return FFH_RES(1);
  }

  len = sep ? s->len + sep->len : s->len;
  if (len > LJ_MAX_STR)
    uj_err_caller(L, UJ_ERR_STROV);
  tlen = len * k;
  if (tlen > LJ_MAX_STR)
    uj_err_caller(L, UJ_ERR_STROV);

  if (tlen == 0) goto empty;
  sb = uj_sbuf_reset_tmp(L);
  uj_sbuf_reserve(sb, tlen);
  /* Paste one string and one separator. */
  uj_sbuf_push_str(sb, s);
  if (sep) {
    tlen -= sep->len;  /* Ignore trailing separator. */
    if (k > 1)
      uj_sbuf_push_str(sb, sep);
  }
  len = uj_sbuf_size(sb);
  for (k--; k > 0; k--)  /* Now copy sbuf content k-1 times. */
    uj_sbuf_push_block(sb, uj_sbuf_front(sb), len);
  setstrV(L, L->base-1, uj_str_new(L, uj_sbuf_front(sb), tlen));
  return FFH_RES(1);
}

LJLIB_ASM(string_reverse)
{
  GCstr *s = uj_lib_checkstr(L, 1);
  uj_sbuf_reserve(uj_sbuf_reset_tmp(L), s->len);
  return FFH_RETRY;
}
LJLIB_ASM_(string_lower)        LJLIB_REC(.)
LJLIB_ASM_(string_upper)        LJLIB_REC(.)

/* ------------------------------------------------------------------------ */

static int writer_buf(lua_State *L, const void *p, size_t size, void *b)
{
  luaL_addlstring((luaL_Buffer *)b, (const char *)p, size);
  UNUSED(L);
  return 0;
}

LJLIB_CF(string_dump)
{
  GCfunc *fn = uj_lib_checkfunc(L, 1);
  int strip = L->base+1 < L->top && tvistruecond(L->base+1);
  luaL_Buffer b;
  L->top = L->base+1;
  luaL_buffinit(L, &b);
  if (!isluafunc(fn) || lj_bcwrite(L, funcproto(fn), writer_buf, &b, strip))
    uj_err_caller(L, UJ_ERR_STRDUMP);
  luaL_pushresult(&b);
  return 1;
}

/* ------------------------------------------------------------------------ */

/* macro to `unsign' a character */
#define uchar(c)        ((unsigned char)(c))

#define CAP_UNFINISHED  (-1)
#define CAP_POSITION    (-2)

typedef struct MatchState {
  const char *src_init;  /* init of source string */
  const char *src_end;  /* end (`\0') of source string */
  lua_State *L;
  int level;  /* total number of captures (finished or unfinished) */
  int depth;
  struct {
    const char *init;
    ptrdiff_t len;
  } capture[LUA_MAXCAPTURES];
} MatchState;

#define L_ESC           '%'

static int check_capture(MatchState *ms, int l)
{
  l -= '1';
  if (l < 0 || l >= ms->level || ms->capture[l].len == CAP_UNFINISHED)
    uj_err_caller(ms->L, UJ_ERR_STRCAPI);
  return l;
}

static int capture_to_close(MatchState *ms)
{
  int level = ms->level;
  for (level--; level>=0; level--)
    if (ms->capture[level].len == CAP_UNFINISHED) return level;
  uj_err_caller(ms->L, UJ_ERR_STRPATC);
  return 0;  /* unreachable */
}

static const char *classend(MatchState *ms, const char *p)
{
  switch (*p++) {
  case L_ESC:
    if (*p == '\0')
      uj_err_caller(ms->L, UJ_ERR_STRPATE);
    return p+1;
  case '[':
    if (*p == '^') p++;
    do {  /* look for a `]' */
      if (*p == '\0')
        uj_err_caller(ms->L, UJ_ERR_STRPATM);
      if (*(p++) == L_ESC && *p != '\0')
        p++;  /* skip escapes (e.g. `%]') */
    } while (*p != ']');
    return p+1;
  default:
    return p;
  }
}

static const unsigned char match_class_map[32] = {
  0,LJ_CHAR_ALPHA,0,LJ_CHAR_CNTRL,LJ_CHAR_DIGIT,0,0,LJ_CHAR_GRAPH,0,0,0,0,
  LJ_CHAR_LOWER,0,0,0,LJ_CHAR_PUNCT,0,0,LJ_CHAR_SPACE,0,
  LJ_CHAR_UPPER,0,LJ_CHAR_ALNUM,LJ_CHAR_XDIGIT,0,0,0,0,0,0,0
};

static int match_class(int c, int cl)
{
  if ((cl & 0xc0) == 0x40) {
    int t = match_class_map[(cl&0x1f)];
    if (t) {
      t = lj_char_isa(c, t);
      return (cl & 0x20) ? t : !t;
    }
    if (cl == 'z') return c == 0;
    if (cl == 'Z') return c != 0;
  }
  return (cl == c);
}

static int matchbracketclass(int c, const char *p, const char *ec)
{
  int sig = 1;
  if (*(p+1) == '^') {
    sig = 0;
    p++;  /* skip the `^' */
  }
  while (++p < ec) {
    if (*p == L_ESC) {
      p++;
      if (match_class(c, uchar(*p)))
        return sig;
    }
    else if ((*(p+1) == '-') && (p+2 < ec)) {
      p+=2;
      if (uchar(*(p-2)) <= c && c <= uchar(*p))
        return sig;
    }
    else if (uchar(*p) == c) return sig;
  }
  return !sig;
}

static int singlematch(int c, const char *p, const char *ep)
{
  switch (*p) {
  case '.': return 1;  /* matches any char */
  case L_ESC: return match_class(c, uchar(*(p+1)));
  case '[': return matchbracketclass(c, p, ep-1);
  default:  return (uchar(*p) == c);
  }
}

static const char *match(MatchState *ms, const char *s, const char *p);

static const char *matchbalance(MatchState *ms, const char *s, const char *p)
{
  if (*p == 0 || *(p+1) == 0)
    uj_err_caller(ms->L, UJ_ERR_STRPATU);
  if (*s != *p) {
    return NULL;
  } else {
    int b = *p;
    int e = *(p+1);
    int cont = 1;
    while (++s < ms->src_end) {
      if (*s == e) {
        if (--cont == 0) return s+1;
      } else if (*s == b) {
        cont++;
      }
    }
  }
  return NULL;  /* string ends out of balance */
}

static const char *max_expand(MatchState *ms, const char *s,
                              const char *p, const char *ep)
{
  ptrdiff_t i = 0;  /* counts maximum expand for item */
  while ((s+i)<ms->src_end && singlematch(uchar(*(s+i)), p, ep))
    i++;
  /* keeps trying to match with the maximum repetitions */
  while (i>=0) {
    const char *res = match(ms, (s+i), ep+1);
    if (res) return res;
    i--;  /* else didn't match; reduce 1 repetition to try again */
  }
  return NULL;
}

static const char *min_expand(MatchState *ms, const char *s,
                              const char *p, const char *ep)
{
  for (;;) {
    const char *res = match(ms, s, ep+1);
    if (res != NULL)
      return res;
    else if (s<ms->src_end && singlematch(uchar(*s), p, ep))
      s++;  /* try with one more repetition */
    else
      return NULL;
  }
}

static const char *start_capture(MatchState *ms, const char *s,
                                 const char *p, int what)
{
  const char *res;
  int level = ms->level;
  if (level >= LUA_MAXCAPTURES) uj_err_caller(ms->L, UJ_ERR_STRCAPN);
  ms->capture[level].init = s;
  ms->capture[level].len = what;
  ms->level = level+1;
  if ((res=match(ms, s, p)) == NULL)  /* match failed? */
    ms->level--;  /* undo capture */
  return res;
}

static const char *end_capture(MatchState *ms, const char *s,
                               const char *p)
{
  int l = capture_to_close(ms);
  const char *res;
  ms->capture[l].len = s - ms->capture[l].init;  /* close capture */
  if ((res = match(ms, s, p)) == NULL)  /* match failed? */
    ms->capture[l].len = CAP_UNFINISHED;  /* undo capture */
  return res;
}

static const char *match_capture(MatchState *ms, const char *s, int l)
{
  size_t len;
  l = check_capture(ms, l);
  len = (size_t)ms->capture[l].len;
  if ((size_t)(ms->src_end-s) >= len &&
      memcmp(ms->capture[l].init, s, len) == 0)
    return s+len;
  else
    return NULL;
}

static const char *match(MatchState *ms, const char *s, const char *p)
{
  if (++ms->depth > LJ_MAX_XLEVEL)
    uj_err_caller(ms->L, UJ_ERR_STRPATX);
  init: /* using goto's to optimize tail recursion */
  switch (*p) {
  case '(':  /* start capture */
    if (*(p+1) == ')')  /* position capture? */
      s = start_capture(ms, s, p+2, CAP_POSITION);
    else
      s = start_capture(ms, s, p+1, CAP_UNFINISHED);
    break;
  case ')':  /* end capture */
    s = end_capture(ms, s, p+1);
    break;
  case L_ESC:
    switch (*(p+1)) {
    case 'b':  /* balanced string? */
      s = matchbalance(ms, s, p+2);
      if (s == NULL) break;
      p+=4;
      goto init;  /* else s = match(ms, s, p+4); */
    case 'f': {  /* frontier? */
      const char *ep; char previous;
      p += 2;
      if (*p != '[')
        uj_err_caller(ms->L, UJ_ERR_STRPATB);
      ep = classend(ms, p);  /* points to what is next */
      previous = (s == ms->src_init) ? '\0' : *(s-1);
      if (matchbracketclass(uchar(previous), p, ep-1) ||
         !matchbracketclass(uchar(*s), p, ep-1)) { s = NULL; break; }
      p=ep;
      goto init;  /* else s = match(ms, s, ep); */
      }
    default:
      if (lj_char_isdigit(uchar(*(p+1)))) {  /* capture results (%0-%9)? */
        s = match_capture(ms, s, uchar(*(p+1)));
        if (s == NULL) break;
        p+=2;
        goto init;  /* else s = match(ms, s, p+2) */
      }
      goto dflt;  /* case default */
    }
    break;
  case '\0':  /* end of pattern */
    break;  /* match succeeded */
  case '$':
    /* is the `$' the last char in pattern? */
    if (*(p+1) != '\0') goto dflt;
    if (s != ms->src_end) s = NULL;  /* check end of string */
    break;
  default: dflt: {  /* it is a pattern item */
    const char *ep = classend(ms, p);  /* points to what is next */
    int m = s<ms->src_end && singlematch(uchar(*s), p, ep);
    switch (*ep) {
    case '?': {  /* optional */
      const char *res;
      if (m && ((res=match(ms, s+1, ep+1)) != NULL)) {
        s = res;
        break;
      }
      p=ep+1;
      goto init;  /* else s = match(ms, s, ep+1); */
      }
    case '*':  /* 0 or more repetitions */
      s = max_expand(ms, s, p, ep);
      break;
    case '+':  /* 1 or more repetitions */
      s = (m ? max_expand(ms, s+1, p, ep) : NULL);
      break;
    case '-':  /* 0 or more repetitions (minimum) */
      s = min_expand(ms, s, p, ep);
      break;
    default:
      if (m) { s++; p=ep; goto init; }  /* else s = match(ms, s+1, ep); */
      s = NULL;
      break;
    }
    break;
    }
  }
  ms->depth--;
  return s;
}

static void push_onecapture(MatchState *ms, int i, const char *s, const char *e)
{
  if (i >= ms->level) {
    if (i == 0)  /* ms->level == 0, too */
      lua_pushlstring(ms->L, s, (size_t)(e - s));  /* add whole match */
    else
      uj_err_caller(ms->L, UJ_ERR_STRCAPI);
  } else {
    ptrdiff_t l = ms->capture[i].len;
    if (l == CAP_UNFINISHED) uj_err_caller(ms->L, UJ_ERR_STRCAPU);
    if (l == CAP_POSITION)
      lua_pushinteger(ms->L, ms->capture[i].init - ms->src_init + 1);
    else
      lua_pushlstring(ms->L, ms->capture[i].init, (size_t)l);
  }
}

static int push_captures(MatchState *ms, const char *s, const char *e)
{
  int i;
  int nlevels = (ms->level == 0 && s) ? 1 : ms->level;
  luaL_checkstack(ms->L, nlevels, "too many captures");
  for (i = 0; i < nlevels; i++)
    push_onecapture(ms, i, s, e);
  return nlevels;  /* number of strings pushed */
}

static int str_find_aux(lua_State *L, int find)
{
  const GCstr *s = uj_lib_checkstr(L, 1);
  const GCstr *p = uj_lib_checkstr(L, 2);
  int32_t init = uj_lib_optint(L, 3, 1);
  int plain = uj_lib_optbool(L, 4);

  const char *sstr = strdata(s);
  const char *needle = strdata(p);
  const char *haystack;

  if (init < 0)
    init += (int32_t)s->len;
  else
    init--;

/*
 * These cases of `init argument` are not documented in Lua Reference Manual,
 * implemented according to vanilla Lua5.1/Lua5.2
 */
  if (init < 0)
    init = 0;

  if (init > s->len) {
#if LJ_52
    setnilV(L->top - 1);
    return 1;
#else
    init = s->len;
#endif
  }
  haystack = sstr + init;

  if (find && (plain || !uj_str_has_pattern_specials(p))) {
    /* Search for fixed string. */
    const char *q = uj_cstr_find(haystack, needle, s->len - init, p->len);
    if (q) {
      setintV(L->top - 2, (int32_t)(q - sstr) + 1);
      setintV(L->top - 1, (int32_t)(q - sstr) + (int32_t)p->len);
      return 2;
    }
  } else {  /* Search for pattern. */
    MatchState ms;
    int anchor = 0;
    if (*needle == '^') {
      needle++;
      anchor = 1;
    }
    ms.L = L;
    ms.src_init = sstr;
    ms.src_end = sstr + s->len;
    do {  /* Loop through string and try to match the pattern. */
      const char *q;
      ms.level = ms.depth = 0;
      q = match(&ms, haystack, needle);
      if (q) {
        if (find) {
          setintV(L->top++, (int32_t)(haystack - (sstr - 1)));
          setintV(L->top++, (int32_t)(q - sstr));
          return push_captures(&ms, NULL, NULL) + 2;
        } else {
          return push_captures(&ms, haystack, q);
        }
      }
    } while (haystack++ < ms.src_end && !anchor);
  }
  setnilV(L->top - 1);  /* Not found. */
  return 1;
}

LJLIB_CF(string_find)           LJLIB_REC(.)
{
  return str_find_aux(L, 1);
}

LJLIB_CF(string_match)
{
  return str_find_aux(L, 0);
}

LJLIB_NOREG LJLIB_CF(string_gmatch_aux)
{
  const char *p = strVdata(uj_lib_upvalue(L, 2));
  GCstr *str = strV(uj_lib_upvalue(L, 1));
  const char *s = strdata(str);
  TValue *tvpos = uj_lib_upvalue(L, 3);
  const char *src = s + tvpos->u32.lo;
  MatchState ms;
  ms.L = L;
  ms.src_init = s;
  ms.src_end = s + str->len;
  for (; src <= ms.src_end; src++) {
    const char *e;
    ms.level = ms.depth = 0;
    if ((e = match(&ms, src, p)) != NULL) {
      int32_t pos = (int32_t)(e - s);
      if (e == src) pos++;  /* Ensure progress for empty match. */
      tvpos->u32.lo = (uint32_t)pos;
      return push_captures(&ms, src, e);
    }
  }
  return 0;  /* not found */
}

LJLIB_CF(string_gmatch)
{
  uj_lib_checkstr(L, 1);
  uj_lib_checkstr(L, 2);
  L->top = L->base+3;
  setnumV(L->top-1, 0);
  uj_lib_pushcc(L, lj_cf_string_gmatch_aux, FF_string_gmatch_aux, 3);
  return 1;
}

static void add_s(MatchState *ms, luaL_Buffer *b, const char *s, const char *e)
{
  size_t l, i;
  const char *news = lua_tolstring(ms->L, 3, &l);
  for (i = 0; i < l; i++) {
    if (news[i] != L_ESC) {
      luaL_addchar(b, news[i]);
    } else {
      i++;  /* skip ESC */
      if (!lj_char_isdigit(uchar(news[i]))) {
        luaL_addchar(b, news[i]);
      } else if (news[i] == '0') {
        luaL_addlstring(b, s, (size_t)(e - s));
      } else {
        push_onecapture(ms, news[i] - '1', s, e);
        luaL_addvalue(b);  /* add capture to accumulated result */
      }
    }
  }
}

static void add_value(MatchState *ms, luaL_Buffer *b,
                      const char *s, const char *e)
{
  lua_State *L = ms->L;
  switch (lua_type(L, 3)) {
    case LUA_TNUMBER:
    case LUA_TSTRING: {
      add_s(ms, b, s, e);
      return;
    }
    case LUA_TFUNCTION: {
      int n;
      lua_pushvalue(L, 3);
      n = push_captures(ms, s, e);
      lua_call(L, n, 1);
      break;
    }
    case LUA_TTABLE: {
      push_onecapture(ms, 0, s, e);
      lua_gettable(L, 3);
      break;
    }
  }
  if (!lua_toboolean(L, -1)) {  /* nil or false? */
    lua_pop(L, 1);
    lua_pushlstring(L, s, (size_t)(e - s));  /* keep original text */
  } else if (!lua_isstring(L, -1)) {
    uj_err_callerv(L, UJ_ERR_STRGSRV, luaL_typename(L, -1));
  }
  luaL_addvalue(b);  /* add result to accumulator */
}

LJLIB_CF(string_gsub)
{
  size_t srcl;
  const char *src = luaL_checklstring(L, 1, &srcl);
  const char *p = luaL_checkstring(L, 2);
  int  tr = lua_type(L, 3);
  int max_s = luaL_optint(L, 4, (int)(srcl+1));
  int anchor = (*p == '^') ? (p++, 1) : 0;
  int n = 0;
  MatchState ms;
  luaL_Buffer b;
  if (!(tr == LUA_TNUMBER || tr == LUA_TSTRING ||
        tr == LUA_TFUNCTION || tr == LUA_TTABLE))
    uj_err_arg(L, UJ_ERR_NOSFT, 3);
  luaL_buffinit(L, &b);
  ms.L = L;
  ms.src_init = src;
  ms.src_end = src+srcl;
  while (n < max_s) {
    const char *e;
    ms.level = ms.depth = 0;
    e = match(&ms, src, p);
    if (e) {
      n++;
      add_value(&ms, &b, src, e);
    }
    if (e && e>src) /* non empty match? */
      src = e;  /* skip it */
    else if (src < ms.src_end)
      luaL_addchar(&b, *src++);
    else
      break;
    if (anchor)
      break;
  }
  luaL_addlstring(&b, src, (size_t)(ms.src_end-src));
  luaL_pushresult(&b);
  lua_pushinteger(L, n);  /* number of substitutions */
  return 2;
}

/* ------------------------------------------------------------------------ */

static void addquoted(lua_State *L, struct sbuf *sb, unsigned int arg)
{
  GCstr *str;
  size_t len;
  const char *s;
  const TValue *tv = uj_lib_narg2tv(L, arg);

  uj_sbuf_push_char(sb, '"');
  if (tvisnum(tv)) {
    uj_sbuf_push_number(sb, numV(tv));
    goto exit;
  }
  str = uj_lib_checkstr(L, arg);
  len = str->len;
  s = strdata(str);
  while (len--) {
    uint32_t c = uchar(*s);
    if (c == '"' || c == '\\' || c == '\n') {
      uj_sbuf_push_char(sb, '\\');
    } else if (lj_char_iscntrl(c)) {  /* This can only be 0-31 or 127. */
      uint32_t d;
      uj_sbuf_push_char(sb, '\\');
      if (c >= 100 || lj_char_isdigit(uchar(s[1]))) {
        uj_sbuf_push_char(sb, '0' + (c >= 100));
        if (c >= 100)
          c -= 100;
        goto tens;
      } else if (c >= 10) {
tens:
        d = (c * 205) >> 11;
        c -= d * 10;
        uj_sbuf_push_char(sb, '0' + d);
      }
      c += '0';
    }
    uj_sbuf_push_char(sb, c);
    s++;
  }
exit:
  uj_sbuf_push_char(sb, '"');
}

/* valid flags in a format specification */
#define FMT_FLAGS       "-+ #0"

static const char *scanformat(lua_State *L, const char *strfrmt, char *form)
{
  const char *p = strfrmt;
  while (*p >= ' ' && *p <= '0' /* fast check */
         && strchr(FMT_FLAGS, *p) != NULL)
    p++;  /* skip flags */
  if ((size_t)(p - strfrmt) >= sizeof(FMT_FLAGS))
    uj_err_caller(L, UJ_ERR_STRFMTR);
  if (lj_char_isdigit(uchar(*p))) p++;  /* skip width */
  if (lj_char_isdigit(uchar(*p))) p++;  /* (2 digits at most) */
  if (*p == '.') {
    p++;
    if (lj_char_isdigit(uchar(*p))) p++;  /* skip precision */
    if (lj_char_isdigit(uchar(*p))) p++;  /* (2 digits at most) */
  }
  if (lj_char_isdigit(uchar(*p)))
    uj_err_caller(L, UJ_ERR_STRFMTW);
  *(form++) = '%';
  strncpy(form, strfrmt, (size_t)(p - strfrmt + 1));
  form += p - strfrmt + 1;
  *form = '\0';
  return p;
}

static void addintlen(char *form)
{
  const size_t intfrmlen_size = sizeof(LUA_INTFRMLEN);
  size_t l = strlen(form);
  char spec = form[l - 1];
  strncpy(form + l - 1, LUA_INTFRMLEN, intfrmlen_size);
  form[l + intfrmlen_size - 2] = spec;
  form[l + intfrmlen_size - 1] = '\0';
}

static GCstr *meta_tostring(lua_State *L, unsigned int arg)
{
  const TValue *mo;
  TValue *o = uj_lib_narg2tv(L, arg);
  lua_assert(o < L->top);  /* Caller already checks for existence. */
  if (LJ_LIKELY(tvisstr(o)))
    return strV(o);
  mo = uj_meta_lookup(L, o, MM_tostring);
  if (!tvisnil(mo)) {
    copyTV(L, L->top++, mo);
    copyTV(L, L->top++, o);
    lua_call(L, uj_mm_narg[MM_tostring], 1);
    L->top--;
    if (tvisstr(L->top))
      return strV(L->top);
    o = uj_lib_narg2tv(L, arg);
    copyTV(L, o, L->top);
  }
  if (tvisnum(o)) {
    return uj_str_fromnumber(L, o->n);
  } else if (tvisnil(o)) {
    return uj_str_newz(L, "nil");
  } else if (tvisfalse(o)) {
    return uj_str_newz(L, "false");
  } else if (tvistrue(o)) {
    return uj_str_newz(L, "true");
  } else {
    struct sbuf sb;
    GCstr *str;

    uj_sbuf_init(L, &sb);
    uj_sbuf_reserve(&sb, 64); /* Should be enough */
    if (tvisfunc(o) && isffunc(funcV(o))) {
      uj_sbuf_push_cstr(&sb, "function: builtin#");
      uj_sbuf_push_int(&sb, funcV(o)->c.ffid);
    } else {
      uj_sbuf_push_cstr(&sb, lj_typename(o));
      uj_sbuf_push_cstr(&sb, ": ");
      uj_sbuf_push_ptr(&sb, lua_topointer(L, arg));
    }
    str = uj_str_frombuf(L, &sb);
    uj_sbuf_free(L, &sb);
    return str;
  }
}

/* Maximum size of each formatted item (> len(format('%99.99f', -1e308))). */
#define MAX_FMTITEM 512
#define push_formatted(sb, format, arg) \
  do { \
    int pushed; \
    uj_sbuf_reserve((sb), uj_sbuf_size(sb) + MAX_FMTITEM); \
    pushed = snprintf(uj_sbuf_back(sb), MAX_FMTITEM, (format), (arg)); \
    lua_assert(pushed >= 0 && pushed < MAX_FMTITEM); \
    (sb)->sz += pushed; \
  } while(0)

LJLIB_CF(string_format)         LJLIB_REC(.)
{
  unsigned int arg = 1;
  const unsigned int top = L->top - L->base;
  GCstr *strfmt = uj_lib_checkstr(L, arg);
  const char *fmt = strdata(strfmt);
  const char *fmt_end = fmt + strfmt->len;
  struct sbuf *sb = uj_sbuf_reset_tmp(L);

  while (fmt < fmt_end) {
    /*
     * Storage for format specifications (such as '%-099.99d').
     * (+10 accounts for %99.99x plus margin of error.)
     */
    char form[sizeof(FMT_FLAGS) + sizeof(LUA_INTFRMLEN) + 10];
    int is_simple_format;

    if (*fmt != L_ESC) {
      const char *start = fmt;

      while(*fmt != L_ESC && fmt < fmt_end)
        fmt++;
      uj_sbuf_push_block(sb, start, fmt - start);
      continue;
    }
    if (*++fmt == L_ESC) {
      uj_sbuf_push_char(sb, *fmt);  /* %% */
      ++fmt;
      continue;
    }

    if (++arg > top)
      luaL_argerror(L, arg, uj_obj_typename[0]);
    fmt = scanformat(L, fmt, form);
    is_simple_format = (form[2] == '\0');
    switch (*fmt++) {
    case 'c': {
      char c = uj_lib_checkint(L, arg);
      if (is_simple_format)
        uj_sbuf_push_char(sb, c);
      else
        push_formatted(sb, form, c);
      break;
    }
    case 'd':  case 'i': {
      LUA_INTFRM_T i = uj_lib_checknum(L, arg);
      if ((i == (int32_t)i) && is_simple_format) {
        uj_sbuf_push_int(sb, i);
      } else {
        addintlen(form);
        push_formatted(sb, form, i);
      }
      break;
    }
    case 'o':  case 'u':  case 'x':  case 'X':
      addintlen(form);
      push_formatted(sb, form, (unsigned LUA_INTFRM_T)uj_lib_checknum(L, arg));
      break;
    case 'e':  case 'E': case 'f': case 'g': case 'G': case 'a': case 'A': {
      lua_Number n = uj_lib_checknum(L, arg);
      if (LJ_LIKELY(lj_fp_finite(n))) {
        push_formatted(sb, form, n);
      } else {
        /* Canonicalize output of non-finite values. */
        char *p, nbuf[UJ_CSTR_NUMBUF];
        size_t len = uj_cstr_fromnum(nbuf, n);
        if (fmt[-1] < 'a') {
          nbuf[len-3] = lj_char_toupper(nbuf[len-3]);
          nbuf[len-2] = lj_char_toupper(nbuf[len-2]);
          nbuf[len-1] = lj_char_toupper(nbuf[len-1]);
        }
        nbuf[len] = '\0';
        for (p = form; *p < 'A' && *p != '.'; p++) ;
        *p++ = 's'; *p = '\0';
        push_formatted(sb, form, nbuf);
      }
      break;
      }
    case 'q':
      addquoted(L, sb, arg);
      break;
    case 'p':
      uj_sbuf_push_ptr(sb, lua_topointer(L, arg));
      break;
    case 's': {
      const TValue *tv = uj_lib_narg2tv(L, arg);
      if (is_simple_format && uj_str_is_coercible(tv)) {
        if (tvisnum(tv))
          uj_sbuf_push_number(sb, numV(tv));
        else
          uj_sbuf_push_str(sb, strV(tv));
      } else {
        GCstr *str = meta_tostring(L, arg);
        if (!strchr(form, '.') && str->len >= 100) {
          /* no precision and string is too long to be formatted;
             keep original string */
          uj_sbuf_push_str(sb, str);
        } else {
          push_formatted(sb, form, strdata(str));
        }
      }
      break;
      }
    default:
      uj_err_callerv(L, UJ_ERR_STRFMTO, *(fmt - 1));
      break;
    }
  }
  setstrV(L, L->top, uj_str_frombuf(L, sb));
  uj_state_stack_incr_top(L);
  lj_gc_check(L);
  return 1;
}

#undef push_formatted
#undef MAX_FMTITEM

/* ------------------------------------------------------------------------ */

#include "lj_libdef.h"

LUALIB_API int luaopen_string(lua_State *L)
{
  GCtab *mt;
  global_State *g;
  LJ_LIB_REG(L, LUA_STRLIBNAME, string);
#if defined(LUA_COMPAT_GFIND)
  lua_getfield(L, -1, "gmatch");
  lua_setfield(L, -2, "gfind");
#endif
  mt = lj_tab_new(L, 0, 1);
  /* NOBARRIER: basemt is a GC root. */
  g = G(L);
  uj_mtab_set_for_type(g, LJ_TSTR, mt);
  settabV(L, lj_tab_setstr(L, mt, uj_meta_name(g, MM_index)), tabV(L->top-1));
  mt->nomm = (uint8_t)(~(1u<<MM_index));
  return 1;
}

