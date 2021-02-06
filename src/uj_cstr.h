/*
 * C String handling.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _UJ_CSTR_H
#define _UJ_CSTR_H

#include <stdint.h>
#include <stddef.h>

#include "lua.h"

typedef union TValue TValue;

#define UJ_CSTR_INTBUF (1 + 10)
#define UJ_CSTR_NUMBUF LUAI_MAXNUMBER2STR

/* Print number to buffer. Canonicalizes non-finite values. */
size_t uj_cstr_fromnum(char *s, lua_Number n);

/* Print integer to buffer. */
size_t uj_cstr_fromint(char *s, int32_t k);

/*
 * Tries to convert a C string buffer `buf` to a number. In case of success,
 * stores the result into `lua_Number` pointed by `n`.
 * Returns 1 if conversion was successful and `n` is properly updated.
 * Returns 0 if conversion failed (`n` is not modified in this case).
 */
int uj_cstr_tonum(const char *buf, lua_Number *n);

/*
 * Tries to convert a C string buffer `buf` to a number. In case of success,
 * stores the result as a numeric payload for `TValue` pointed by `tv`
 * and sets an appropriate tag for this `TValue`.
 * Returns 1 if conversion was successful and `tv` is properly updated.
 * Returns 0 if conversion failed (`tv` is not modified in this case).
 */
int uj_cstr_tonumtv(const char *buf, TValue *tv);

/*
 * Locate substring `needle` in a `haystack`.
 * Return NULL if needlelen greater than haystacklen,
 * return haystack if needle is empty
 */
const char *uj_cstr_find(const char *haystack, const char *needle,
			 size_t haystacklen, size_t needlelen);

#endif /* !_UJ_CSTR_H */
