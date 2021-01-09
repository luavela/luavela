/*
 * Dynamically resizeable buffer.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJ_SBUF_H
#define _UJ_SBUF_H

#include "lj_obj.h"

/* -- Interfaces for initializing the buffer and managing its storage. ------ */

static LJ_AINLINE void uj_sbuf_init(struct lua_State *L, struct sbuf *sb)
{
	memset(sb, 0, sizeof(*sb));
	sb->L = L;
}

/* Request for total capacity of at least `n` bytes. May cause re-allocation. */
void uj_sbuf_reserve(struct sbuf *sb, size_t n);

static LJ_AINLINE void uj_sbuf_free(const struct lua_State *L, struct sbuf *sb)
{
	/* sb->L may be already GC'ed, don't use it to get mem_manager. */
	uj_mem_free(MEM(L), sb->buf, sb->cap);
}

static LJ_AINLINE void uj_sbuf_reset(struct sbuf *sb)
{
	sb->sz = 0;
}

/* -- Working with global string buffer for temporaries. -------------------- */

/*
 * Initializes global buffer for working with `L`.
 * Returns tmp buf itself for convinience.
 */
static LJ_AINLINE struct sbuf *uj_sbuf_reset_tmp(struct lua_State *L)
{
	struct sbuf *sb = &G(L)->tmpbuf;

	sb->L = L;
	uj_sbuf_reset(sb);
	return sb;
}

/*
 * Reserves `n` bytes inside tmpbuf through `L` and returns pointer to
 * underlying memory for direct access.
 * Prefer using high-level 'push' interfaces over this one.
 */
static LJ_AINLINE char *uj_sbuf_tmp_bytes(struct lua_State *L, size_t n)
{
	struct sbuf *sb = &G(L)->tmpbuf;

	sb->L = L;
	uj_sbuf_reserve(sb, n);
	return sb->buf;
}

/* Tries to shrink global tmp buffer */
void uj_sbuf_shrink_tmp(struct lua_State *L);

/* -- Access to underlying data. -------------------------------------------- */

static LJ_AINLINE size_t uj_sbuf_size(const struct sbuf *sb)
{
	return sb->sz;
}

static LJ_AINLINE char *uj_sbuf_front(const struct sbuf *sb)
{
	return sb->buf;
}

static LJ_AINLINE char *uj_sbuf_back(const struct sbuf *sb)
{
	return &sb->buf[sb->sz];
}

static LJ_AINLINE char *uj_sbuf_at(const struct sbuf *sb, size_t i)
{
	return &sb->buf[i];
}

/* -- Push interfaces. These automatically extend storage if needed. -------- */

struct sbuf *uj_sbuf_push_char(struct sbuf *sb, char c);
struct sbuf *uj_sbuf_push_int(struct sbuf *sb, int32_t n);
struct sbuf *uj_sbuf_push_num(struct sbuf *sb, lua_Number n);
/* If possible, converts number to int32_t without rounding before pushing. */
struct sbuf *uj_sbuf_push_number(struct sbuf *sb, lua_Number n);
/* Always converts number to integer type before pushing. */
struct sbuf *uj_sbuf_push_numint(struct sbuf *sb, lua_Number n);
struct sbuf *uj_sbuf_push_uleb128(struct sbuf *sb, uint64_t value);
struct sbuf *uj_sbuf_push_ptr(struct sbuf *sb, const void *ptr);
struct sbuf *uj_sbuf_push_block(struct sbuf *sb, const void *src, size_t n);

struct sbuf *uj_sbuf_push_str(struct sbuf *sb, const struct GCstr *s);

static LJ_AINLINE struct sbuf *uj_sbuf_push_cstr(struct sbuf *sb,
						 const char *cstr)
{
	return uj_sbuf_push_block(sb, cstr, strlen(cstr));
}

#endif /* !_UJ_SBUF_H */
