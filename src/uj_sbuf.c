/*
 * Implementation of dynamically resizeable buffer.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <stdio.h>

#include "uj_cstr.h"
#include "uj_mem.h"
#include "uj_sbuf.h"
#include "utils/leb128.h"
#include "uj_err.h"

static LJ_AINLINE size_t sbuf_capacity(const struct sbuf *sb)
{
	return sb->cap;
}

static void sbuf_realloc(struct sbuf *sb, size_t n)
{
	sb->buf = uj_mem_realloc(sb->L, sb->buf, sb->cap, n);
	sb->cap = n;
}

static LJ_AINLINE void sbuf_fit(struct sbuf *sb, size_t add)
{
	uj_sbuf_reserve(sb, sb->sz + add);
}

void uj_sbuf_shrink_tmp(struct lua_State *L)
{
	struct sbuf *sb = &G(L)->tmpbuf;
	size_t new_capacity = sbuf_capacity(sb) / 2;

	if (new_capacity < uj_sbuf_size(sb))
		return;
	if (new_capacity < LJ_MIN_SBUF)
		return;

	sb->L = L; /* Update sb->L, which may be already GC'ed. */
	sbuf_realloc(sb, new_capacity);
}

void uj_sbuf_reserve(struct sbuf *sb, size_t n)
{
	size_t cap;

	if (n <= sbuf_capacity(sb))
		return;

	if (LJ_UNLIKELY(n > LJ_MAX_SBUF))
		uj_err(sb->L, UJ_ERR_SBUFOV);

	cap = sbuf_capacity(sb);
	if (cap < LJ_MIN_SBUF)
		cap = LJ_MIN_SBUF;

	while (cap < n)
		cap *= 2;

	sbuf_realloc(sb, cap);
}

struct sbuf *uj_sbuf_push_char(struct sbuf *sb, char c)
{
	sbuf_fit(sb, 1);
	sb->buf[sb->sz++] = c;
	return sb;
}

struct sbuf *uj_sbuf_push_int(struct sbuf *sb, int32_t n)
{
	sbuf_fit(sb, UJ_CSTR_INTBUF);
	sb->sz += uj_cstr_fromint(uj_sbuf_back(sb), n);
	return sb;
}

struct sbuf *uj_sbuf_push_num(struct sbuf *sb, lua_Number n)
{
	sbuf_fit(sb, UJ_CSTR_NUMBUF);
	sb->sz += uj_cstr_fromnum(uj_sbuf_back(sb), n);
	return sb;
}

struct sbuf *uj_sbuf_push_number(struct sbuf *sb, lua_Number n)
{
	int32_t intnum = lj_num2int(n);

	if (intnum == n)
		return uj_sbuf_push_int(sb, intnum);

	return uj_sbuf_push_num(sb, n);
}

struct sbuf *uj_sbuf_push_numint(struct sbuf *sb, lua_Number n)
{
	int64_t intnum = (int64_t)n;

	if (checki32(intnum))
		return uj_sbuf_push_int(sb, intnum);

	sbuf_fit(sb, LUAI_MAXNUMBER2STR);
	sb->sz += sprintf(uj_sbuf_back(sb), "%" PRId64, intnum);
	return sb;
}

struct sbuf *uj_sbuf_push_ptr(struct sbuf *sb, const void *ptr)
{
	size_t i, lasti;
	ptrdiff_t p = (ptrdiff_t)ptr;
	ptrdiff_t hi_p = p >> 32;
	char *buf;

	if (ptr == NULL)
		return uj_sbuf_push_cstr(sb, "NULL");

	/* '0x' + two hex symbols per byte */
	sbuf_fit(sb, 2 + 2 * sizeof(ptr));
	buf = uj_sbuf_back(sb);
	lasti = 2 + 2 * 4;
	/* Shorten leading zeros for 64 bit pointers. */
	if (hi_p != 0)
		lasti += 1 + (lj_bsr((uint32_t)hi_p) >> 2);
	buf[0] = '0';
	buf[1] = 'x';
	for (i = lasti - 1; i >= 2; i--, p >>= 4)
		buf[i] = "0123456789abcdef"[(p & 15)];
	sb->sz += lasti;
	return sb;
}

struct sbuf *uj_sbuf_push_uleb128(struct sbuf *sb, uint64_t value)
{
	sbuf_fit(sb, LEB128_U64_MAXSIZE);
	sb->sz += write_uleb128((uint8_t *)uj_sbuf_back(sb), value);
	return sb;
}

struct sbuf *uj_sbuf_push_block(struct sbuf *sb, const void *src, size_t n)
{
	sbuf_fit(sb, n);
	memcpy(uj_sbuf_back(sb), src, n);
	sb->sz += n;
	return sb;
}

struct sbuf *uj_sbuf_push_str(struct sbuf *sb, const struct GCstr *s)
{
	return uj_sbuf_push_block(sb, strdata(s), s->len);
}
