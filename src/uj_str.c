/*
 * String handling.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 *
 * Portions taken verbatim or adapted from the Lua interpreter.
 * Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
 */

#include "uj_obj_immutable.h"
#include "lj_gc.h"
#include "uj_mem.h"
#include "uj_err.h"
#include "uj_str.h"
#include "uj_sbuf.h"
#include "uj_strhash.h"
#include "uj_state.h"
#include "utils/strhash.h"
#include "utils/lj_char.h"

/* -- String helpers ------------------------------------------------------ */

int uj_str_has_pattern_specials(const GCstr *s)
{
	const char *p = strdata(s);
	const char *q = p + s->len;
	const char *specials = "^$*+?.([%-";

	while (p != q) {
		/* uint8_t needed by lj_char_is* interfaces */
		const uint8_t c = *(const uint8_t *)p++;

		if (lj_char_ispunct(c) /* Fast check */
		    && strchr(specials, c)) /* Exact check */
			return 1; /* Found a pattern matching char. */
	}
	return 0; /* No pattern matching chars found. */
}

static LJ_AINLINE GCstr *str_flip_case(lua_State *L, const GCstr *s, char low,
				       char hi)
{
	size_t len = s->len;
	char *buf = uj_sbuf_tmp_bytes(L, len);
	const char *src = strdata(s);
	size_t i;

	for (i = 0; i < len; ++i) {
		char c = src[i];

		buf[i] = (c >= low && c <= hi) ? lj_char_flipcase(c) : c;
	}
	return uj_str_new(L, buf, len);
}

GCstr *uj_str_lower(lua_State *L, const GCstr *s)
{
	return str_flip_case(L, s, 'A', 'Z');
}

GCstr *uj_str_upper(lua_State *L, const GCstr *s)
{
	return str_flip_case(L, s, 'a', 'z');
}

/* -- String interning ---------------------------------------------------- */

/* Ordered compare of strings. Assumes string data is 4-byte aligned. */
int32_t uj_str_cmp(const GCstr *a, const GCstr *b)
{
	size_t i;
	size_t n = a->len > b->len ? b->len : a->len;

	for (i = 0; i < n; i += 4) {
		/* Note: innocuous access up to end of string + 3. */
		uint32_t va = *(const uint32_t *)(strdata(a) + i);
		uint32_t vb = *(const uint32_t *)(strdata(b) + i);

		if (va == vb)
			continue;

		va = lj_bswap(va);
		vb = lj_bswap(vb);
		i -= n;
		if ((int32_t)i >= -3) {
			va >>= 32 + (i << 3);
			vb >>= 32 + (i << 3);
		}
		if (va == vb)
			break;
		return va < vb ? -1 : 1;
	}
	return (int32_t)(a->len - b->len);
}

/* Intern a string and return string object. */
GCstr *uj_str_new(lua_State *L, const char *str, size_t lenx)
{
	global_State *g;
	uj_strhash_t *strhash;
	GCstr *s;
	/*
	 * Initially len, a, b and h had type MSize and MSize had type
	 * uint32_t. When expanding MSize to size_t, outcome of hash function
	 * and table traversal order (as seen through API) changed. Hardcoded
	 * uint32_t below is a workaround to maintain original
	 * insertion/traversal order.
	 */
	uint32_t hash;

	g = G(L);
	strhash = gl_strhash(g);

	if (lenx >= LJ_MAX_STR)
		uj_err(L, UJ_ERR_STROV);
	if (lenx == 0)
		return g->strempty;

	/*
	 * Calculate hash of the string. Various hashing algorithms
	 * are available, see utils/strhash for all possibilities.
	 *
	 * hash = strhash_city(str, lenx);
	 * hash = strhash_luajit2(str, (uint32_t)lenx);
	 * hash = strhash_murmur3(str, (uint32_t)lenx);
	 */
	hash = g->hashf(str, (uint32_t)lenx);

	/* Check if the string has already been interned. */
	s = uj_strhash_find(strhash, str, lenx, hash);
	if (NULL == s)
		s = uj_strhash_find(gl_strhash_sealed(g), str, lenx, hash);

	if (NULL != s) {
		g->strhash_hit++;
		GCobj *o = obj2gco(s);
		/*
		 * Resurrect if dead.
		 * Can only happen with fixstring() (keywords).
		 */
		if (isdead(g, o))
			flipwhite(o);
		return s;
	}

	/* Nope, create a new string. */
	g->strhash_miss++;
	s = uj_mem_alloc(L, sizeof(GCstr) + lenx + 1);
	newwhite(g, s);
	s->gct = ~LJ_TSTR;
	s->len = lenx;
	s->hash = hash;
	s->reserved = 0;
	uj_obj_immutable_set_mark(obj2gco(s));
	memcpy(strdatawr(s), str, lenx);
	strdatawr(s)[lenx] = '\0'; /* Zero-terminate string. */

	uj_strhash_add(strhash, L, s); /* Add it to string hash table. */

	return s; /* Return newly interned string. */
}

void uj_str_free(global_State *g, GCstr *s)
{
	lua_assert(0 != s->len);
	lua_assert(!uj_obj_is_sealed(obj2gco(s)));

	uj_strhash_t *strhash = g->strhash_sweep;

	strhash->count--;
	uj_mem_free(MEM_G(g), s, uj_str_sizeof(s));
}

GCstr *uj_str_copy(lua_State *L, const GCstr *src)
{
	GCstr *dst = uj_str_new(L, strdata(src), src->len);

	if (src->marked & LJ_GC_FIXED)
		fixstring(dst);
	return dst;
}

GCstr *uj_str_frombuf(lua_State *L, const struct sbuf *sb)
{
	return uj_str_new(L, uj_sbuf_front(sb), uj_sbuf_size(sb));
}

/* -- Type conversions ---------------------------------------------------- */

/* Convert number to string. */
static GCstr *str_fromnum(lua_State *L, lua_Number n)
{
	char buf[UJ_CSTR_NUMBUF];
	size_t len = uj_cstr_fromnum(buf, n);

	return uj_str_new(L, buf, len);
}

/* Convert integer to string. */
GCstr *uj_str_fromint(lua_State *L, int32_t k)
{
	char s[UJ_CSTR_INTBUF];
	size_t len = uj_cstr_fromint(s, k);

	return uj_str_new(L, s, len);
}

GCstr *uj_str_fromnumber(lua_State *L, lua_Number n)
{
	int32_t intnum = lj_num2int(n);

	if (n == (lua_Number)intnum)
		return uj_str_fromint(L, intnum);
	return str_fromnum(L, n);
}

/* -- String formatting --------------------------------------------------- */

static void str_handle_specifier(struct sbuf *sb, char specifier, va_list argp)
{
	/* This function only handles %s, %c, %d, %f and %p specifiers. */
	switch (specifier) {
	case 's': {
		const char *s = va_arg(argp, char *);

		if (s == NULL)
			s = "(null)";
		uj_sbuf_push_cstr(sb, s);
		break;
	}
	case 'c':
		uj_sbuf_push_char(sb, (char)va_arg(argp, int));
		break;
	case 'd':
		uj_sbuf_push_int(sb, va_arg(argp, int32_t));
		break;
	case 'f':
		uj_sbuf_push_num(sb, va_arg(argp, LUAI_UACNUMBER));
		break;
	case 'p':
		uj_sbuf_push_ptr(sb, va_arg(argp, void *));
		break;
	case '%':
		uj_sbuf_push_char(sb, '%');
		break;
	default:
		uj_sbuf_push_char(sb, '%');
		uj_sbuf_push_char(sb, specifier);
		break;
	}
}

/* Push formatted message as a string object to Lua stack. va_list variant. */
const char *uj_str_pushvf(lua_State *L, const char *fmt, va_list argp)
{
	struct sbuf *sb = uj_sbuf_reset_tmp(L);

	uj_sbuf_reserve(sb, strlen(fmt));
	for (;;) {
		const char *e = strchr(fmt, '%');

		if (e == NULL)
			break;
		uj_sbuf_push_block(sb, fmt, (size_t)(e - fmt));
		str_handle_specifier(sb, e[1], argp);
		fmt = e + 2;
	}
	uj_sbuf_push_cstr(sb, fmt);
	setstrV(L, L->top, uj_str_frombuf(L, sb));
	uj_state_stack_incr_top(L);
	return strVdata(L->top - 1);
}

/* Push formatted message as a string object to Lua stack. Vararg variant. */
const char *uj_str_pushf(lua_State *L, const char *fmt, ...)
{
	const char *msg;
	va_list argp;

	va_start(argp, fmt);
	msg = uj_str_pushvf(L, fmt, argp);
	va_end(argp);
	return msg;
}

/* -- String scanning ----------------------------------------------------- */

int uj_str_tonum(const GCstr *str, lua_Number *n)
{
	return uj_cstr_tonum(strdata(str), n);
}

/* -- String concatenation for interpreted code -------------------------- */

/*
 * Performs naive concatenation of slots from `top` to `bottom` (both inclusive)
 * assuming that each slot is either a string or a number. Throws if the result
 * of a concatenation is too large. Returns a pointer to interned resulting
 * string.
 */
static GCstr *str_cat(lua_State *L, TValue *bottom, TValue *top)
{
	ptrdiff_t i;
	size_t buf_len = 0;
	struct sbuf *sb = uj_sbuf_reset_tmp(L);
	const ptrdiff_t nslots = top - bottom + 1;

	lua_assert(nslots > 1);

	for (i = 0; i < nslots; i++) {
		const TValue *tv = &bottom[i];
		const size_t len = tvisstr(tv) ? strV(tv)->len
					       : LUAI_MAXNUMBER2STR;

		lua_assert(uj_str_is_coercible(tv));

		if (LJ_UNLIKELY(len >= LJ_MAX_STR - buf_len))
			uj_err(L, UJ_ERR_STROV);
		buf_len += len;
	}

	uj_sbuf_reserve(sb, buf_len);
	for (i = 0; i < nslots; i++) {
		const TValue *tv = &bottom[i];

		if (tvisnum(tv))
			uj_sbuf_push_number(sb, numV(tv));
		else
			uj_sbuf_push_str(sb, strV(tv));
	}

	return uj_str_frombuf(L, sb);
}

#ifdef NDEBUG
static void str_assert_cat_substack(const TValue *bottom, const TValue *newtop,
				    const TValue *top)
{
	UNUSED(bottom);
	UNUSED(newtop);
	UNUSED(top);
}
#else
static void str_assert_cat_substack(const TValue *bottom, const TValue *newtop,
				    const TValue *top)
{
	const TValue *slot;

	lua_assert(newtop >= bottom);
	lua_assert(top - newtop >= 1);

	for (slot = newtop; slot <= top; slot++)
		lua_assert(uj_str_is_coercible(slot));
}
#endif /* NDEBUG */

/*
 * NB! It is *NOT* guaranteed that any slot in the range [bottom; top] is left
 * intact during concatenation.
 */

TValue *uj_str_cat_step(lua_State *L, TValue *bottom, TValue *top)
{
	TValue *newtop = top;
	GCstr *str;

	lua_assert(top >= bottom);

	while ((newtop >= bottom) && uj_str_is_coercible(newtop))
		newtop--;
	newtop++;

	if (newtop >= top)
		return top;

	str_assert_cat_substack(bottom, newtop, top);

	str = str_cat(L, newtop, top);
	setstrV(L, newtop, str);

	return newtop;
}

static LJ_AINLINE int str_isspace(const GCstr *str, size_t i)
{
	lua_assert(i < str->len);
	return lj_char_isspace((unsigned char)strdata(str)[i]);
}

GCstr *uj_str_trim(lua_State *L, const GCstr *str)
{
	size_t left = 0;
	size_t right = str->len - 1;

	/* empty string - nothing to do */
	if (str->len == 0)
		return G(L)->strempty;

	/* find the first non-space char from the left */
	while ((left < str->len) && str_isspace(str, left))
		left++;

	/* string consists of whitespace chars only */
	if (left == str->len)
		return G(L)->strempty;

	/* find the first non-space char from the right */
	while (str_isspace(str, right))
		right--;

	return uj_str_new(L, strdata(str) + left, (right - left + 1));
}

/* -- String concatenation for JIT-compiled code -------------------------- */

#if LJ_HASJIT

/*
 * Performs naive concatenation of two strings.
 * Returns a pointer to interned resulting string.
 */
GCstr *uj_str_cat_fold(lua_State *L, const GCstr *str1, const GCstr *str2)
{
	size_t res_len = str1->len + str2->len;
	struct sbuf *sb = uj_sbuf_reset_tmp(L);

	uj_sbuf_reserve(sb, res_len);
	uj_sbuf_push_str(sb, str1);
	uj_sbuf_push_str(sb, str2);

	lua_assert(res_len == uj_sbuf_size(sb));

	return uj_str_frombuf(L, sb);
}

#endif /* LJ_HASJIT */
