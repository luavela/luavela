/*
 * String handling.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _UJ_STR_H
#define _UJ_STR_H

#include "lj_obj.h"
#include "uj_cstr.h"

/* String helpers. */

/* Check whether a string has a pattern matching special character. */
int uj_str_has_pattern_specials(const GCstr *s);

/* Returns a copy of string with lowered case. */
GCstr *uj_str_lower(lua_State *L, const GCstr *s);

/* Returns a copy of string with uppered case. */
GCstr *uj_str_upper(lua_State *L, const GCstr *s);

/* String interning. */
int32_t uj_str_cmp(const GCstr *a, const GCstr *b);
GCstr *uj_str_new(lua_State *L, const char *str, size_t len);
void uj_str_free(global_State *g, GCstr *s);

GCstr *uj_str_frombuf(lua_State *L, const struct sbuf *sb);

static LJ_AINLINE size_t uj_str_sizeof(const GCstr *s)
{
	return sizeof(GCstr) + s->len + 1;
}

static LJ_AINLINE GCstr *uj_str_newz(lua_State *L, const char *str)
{
	return uj_str_new(L, str, strlen(str));
}

/*
 * Deep copy if G(`L`) and the owner of `str` are different global states,
 * does nothing but return `str` otherwise.
 */
GCstr *uj_str_copy(lua_State *L, const GCstr *src);

/* Type conversions. */
GCstr *uj_str_fromint(lua_State *L, int32_t k);
GCstr *uj_str_fromnumber(lua_State *L, lua_Number n);

/* String formatting. */
const char *uj_str_pushvf(lua_State *L, const char *fmt, va_list argp);
__attribute__((format(printf, 2, 3))) const char *
uj_str_pushf(lua_State *L, const char *fmt, ...);

/*
 * Tries to convert payload of `GCstr *str` to a number. In case of success,
 * stores the result into `lua_Number` pointed by `n`.
 * Returns 1 if conversion was successful and `n` is properly updated.
 * Returns 0 if conversion failed (`n` is not modified in this case).
 */
int uj_str_tonum(const GCstr *str, lua_Number *n);

/*
 * Tries to convert payload of `GCstr *str` to a number. In case of success,
 * stores the result as a numeric payload for `TValue` pointed by `tv`
 * and sets an appropriate tag for this `TValue`.
 * Returns 1 if conversion was successful and `tv` is properly updated.
 * Returns 0 if conversion failed (`tv` is not modified in this case).
 */
static LJ_AINLINE int uj_str_tonumtv(const GCstr *str, TValue *tv)
{
	return uj_cstr_tonumtv(strdata(str), tv);
}

/* Check for number or convert string to number/int in-place (!). */
static LJ_AINLINE int uj_str_tonumber(TValue *tv)
{
	return tvisnum(tv) || (tvisstr(tv) && uj_str_tonumtv(strV(tv), tv));
}

/* Checks if a TValue can be coerced to a string. */
static LJ_AINLINE int uj_str_is_coercible(const TValue *tv)
{
	return tvisstr(tv) || tvisnum(tv) ? 1 : 0;
}

/*
 * Performs naive concatenation of slots from `top` to `bottom` (both inclusive)
 * as long as no incoercible slots are observed. Returns a pointer to the new
 * concatenation stack top. If this value is equal to `bottom`, concatenation
 * is fully done, and the result is stored in `bottom`. Otherwise a __concat
 * metamethod should be invoked for `top` and `top - 1` slots.
 */
TValue *uj_str_cat_step(lua_State *L, TValue *bottom, TValue *top);

/* Removes whitespace from both ends of a string. */
GCstr *uj_str_trim(lua_State *L, const GCstr *str);

#if LJ_HASJIT

/* Helper interface to concatenate strings during IR folding. */
GCstr *uj_str_cat_fold(lua_State *L, const GCstr *str1, const GCstr *str2);

#endif /* LJ_HASJIT */

#endif /* !_UJ_STR_H */
