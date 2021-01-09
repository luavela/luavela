/*
 * C String handling.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 *
 * Portions taken verbatim or adapted from the Lua interpreter.
 * Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
 */

#include <stdio.h>

#include "uj_cstr.h"
#include "lj_obj.h"
#include "utils/fp.h"
#include "utils/strscan.h"

size_t uj_cstr_fromnum(char *s, lua_Number n)
{
	switch (uj_fp_classify(n)) {
	case LJ_FP_NAN:
		s[0] = 'n';
		s[1] = 'a';
		s[2] = 'n';
		return 3;
	case LJ_FP_PINF:
		s[0] = 'i';
		s[1] = 'n';
		s[2] = 'f';
		return 3;
	case LJ_FP_MINF:
		s[0] = '-';
		s[1] = 'i';
		s[2] = 'n';
		s[3] = 'f';
		return 4;
	default: /* == LJ_FP_FINITE */
		return (size_t)lua_number2str(s, n);
	}
}

static LJ_AINLINE char *cstr_print_four_digits(char *s, uint32_t u, int padded)
{
	if (!padded) {
		if (u < 10)
			goto dig1;
		if (u < 100)
			goto dig2;
		if (u < 1000)
			goto dig3;
	}
	*s++ = '0' + u / 1000;
	u %= 1000;
dig3:
	*s++ = '0' + u / 100;
	u %= 100;
dig2:
	*s++ = '0' + u / 10;
	u %= 10;
dig1:
	*s++ = '0' + u;

	return s;
}

static LJ_AINLINE char *cstr_print_two_digits(char *s, uint32_t u)
{
	if (u >= 10) {
		*s++ = '0' + u / 10;
		u %= 10;
	}
	*s++ = '0' + u;
	return s;
}

size_t uj_cstr_fromint(char *s, int32_t k)
{
	uint32_t u = k;
	uint32_t hi, med, lo;
	char *p = s;

	if (k < 0) {
		u = -k;
		*p++ = '-';
	}

	if (u < 10000)
		return cstr_print_four_digits(p, u, 0) - s;

	med = u / 10000;
	lo = u % 10000;
	if (med < 10000) {
		p = cstr_print_four_digits(p, med, 0);
		return cstr_print_four_digits(p, lo, 1) - s;
	}

	hi = med / 10000;
	med %= 10000;
	p = cstr_print_two_digits(p, hi);
	p = cstr_print_four_digits(p, med, 1);
	return cstr_print_four_digits(p, lo, 1) - s;
}

int uj_cstr_tonum(const char *buf, lua_Number *n)
{
	double d;
	StrScanFmt fmt;

	fmt = strscan_tonumber((const uint8_t *)buf, &d, STRSCAN_OPT_TONUM);
	lua_assert(fmt == STRSCAN_ERROR || fmt == STRSCAN_NUM);
	if (fmt != STRSCAN_ERROR)
		*n = d;
	return fmt != STRSCAN_ERROR;
}

int uj_cstr_tonumtv(const char *buf, TValue *tv)
{
	lua_Number tmp;
	int converted = uj_cstr_tonum(buf, &tmp);

	if (converted)
		setnumV(tv, tmp);
	return converted;
}

/*
 * NB: Cannot use `strstr` as \0 is a valid character in Lua strings
 * and cannot use `memmem` as it is a GNU extension, not standard C
 */
const char *uj_cstr_find(const char *haystack, const char *needle,
			 size_t haystacklen, size_t needlelen)
{
	char c;

	if (needlelen > haystacklen)
		return NULL;

	if (needlelen == 0)
		return haystack;

	c = *needle++;
	needlelen--;
	haystacklen -= needlelen;
	while (haystacklen) {
		const char *p = memchr(haystack, c, haystacklen);
		if (p == NULL)
			break;
		if (memcmp(p + 1, needle, needlelen) == 0)
			return p;
		p++;
		haystacklen -= (size_t)(p - haystack);
		haystack = p;
	}
	return NULL;
}
