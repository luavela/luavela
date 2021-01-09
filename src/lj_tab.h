/*
 * Table handling.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _LJ_TAB_H
#define _LJ_TAB_H

#include "lj_obj.h"

/* Hash constants. Tuned using a brute force search. */
#define HASH_BIAS       (-0x04c11db7)
#define HASH_ROT1       14
#define HASH_ROT2       5
#define HASH_ROT3       13

/* Scramble the bits of numbers and pointers. */
static LJ_AINLINE uint32_t hashrot(uint32_t lo, uint32_t hi)
{
  /* Prefer variant that compiles well for a 2-operand CPU. */
  lo ^= hi; hi = lj_rol(hi, HASH_ROT1);
  lo -= hi; hi = lj_rol(hi, HASH_ROT2);
  hi ^= lo; hi -= lj_rol(lo, HASH_ROT3);
  return hi;
}

#define hsize2hbits(s)  ((s) ? ((s)==1 ? 1 : 1+lj_bsr((uint32_t)((s)-1))) : 0)

GCtab *lj_tab_new(lua_State *L, uint32_t asize, uint32_t hbits);
#if LJ_HASJIT
GCtab * lj_tab_new_jit(lua_State *L, uint32_t ahsize);
#endif
GCtab * lj_tab_dup(lua_State *L, const GCtab *kt);
void lj_tab_free(global_State *g, GCtab *t);
#if LJ_HASFFI
void lj_tab_rehash(lua_State *L, GCtab *t);
#endif
void lj_tab_reasize(lua_State *L, GCtab *t, uint32_t nasize);

/* Caveat: all getters except lj_tab_get() can return NULL! */

const TValue * lj_tab_getinth(const GCtab *t, int32_t key);
const TValue *lj_tab_getstr(const GCtab *t, const GCstr *key);
const TValue *lj_tab_get(lua_State *L, GCtab *t, const TValue *key);

/* Caveat: all setters require a write barrier for the stored value. */

TValue *lj_tab_newkey(lua_State *L, GCtab *t, const TValue *key);
TValue *lj_tab_setint(lua_State *L, GCtab *t, int32_t key);
TValue *lj_tab_setstr(lua_State *L, GCtab *t, GCstr *key);
TValue *lj_tab_set(lua_State *L, GCtab *t, const TValue *key);

#define inarray(t, key)         ((size_t)(key) < (size_t)(t)->asize)
#define arrayslot(t, i)         (&((t)->array)[(i)])
#define lj_tab_getint(t, key) \
  (inarray((t), (key)) ? arrayslot((t), (key)) : lj_tab_getinth((t), (key)))

uint32_t lj_tab_nexta(const GCtab *t, uint32_t key);
const Node *lj_tab_nexth(lua_State *L, const GCtab *t, const Node *n);

/* Advance to the next step in a table traversal. Iterator's state is derived
** from `key`, the new pair (if any) is stored in `key` and `key` + 1
** (standard lua_next behaviour as per the Lua 5.1 Reference Manual).
*/
int lj_tab_next(lua_State *L, const GCtab *t, TValue *key);

/* Advance to the next step in a table traversal. Iterator's state is derived
** from the *internal storage index* `key`. The new pair is pushed onto the
** stack, the next internal storage index is returned. If the entire `t` is
** traversed, does not touch the stack and returns 0.
*/
uint32_t lj_tab_iterate(lua_State *L, const GCtab *t, uint32_t key);

/* Version of lj_tab_iterate to be used on a trace */
uint32_t lj_tab_iterate_jit(const GCtab *t, uint32_t key);

size_t lj_tab_len(const GCtab *t);
size_t lj_tab_size(const GCtab *t);
size_t lj_tab_sizeof(const GCtab *t);

/* Abstract marking of "parts" of a table t. */
void lj_tab_traverse(lua_State *L, GCtab *t, gco_mark_flipper marker);

/*
 * Creates a new table with a non-empty array part only
 * with values equal to keys of the source table.
 */
GCtab *lj_tab_keys(lua_State *L, const GCtab *src);

/*
 * Creates a new table with a non-empty array part only
 * with values equal to values of the source table.
 */
GCtab *lj_tab_values(lua_State *L, const GCtab *src);

/*
 * Creates a new table with keys equal to values of the array part to the
 * first nil of the source table (as 'ipairs') and values all set to 'true'.
 */
GCtab *lj_tab_toset(lua_State *L, const GCtab *src);

/*
 * Deep copies a table (possibly from another global_State),
 * `src` table may contain only non-gcv, strings, tables
 * or Lua functions without upvalues and accesses to globals.
 * `src` table is temporarily marked with UJ_GCO_TMPMARK during copying.
 */
GCtab *lj_tab_deepcopy(lua_State *L, GCtab *src, struct deepcopy_ctx *ctx);

/* Interface to clear marks made on deep copying */
void lj_tab_clear_mark(lua_State *L, GCobj *o);

/* Implements table.concat, but returns NULL on error */
GCstr *lj_tab_concat(lua_State *L, const GCtab *t, const GCstr *sep,
                     int32_t start, int32_t end, int32_t *fail);

/*
 * Recursively indexes the table at &base[0] with the rest of n - 1 arguments.
 * Does not respect metamethods, but returns NULL in case metamethod semantics
 * must be respected at some nesting level. Also see uj_meta_rindex.
 */
const TValue *lj_tab_rawrindex(lua_State *L, const TValue *base, size_t n);

#if LJ_HASJIT
/* The same as lj_tab_rawrindex, to be invoked from a trace. */
const TValue *lj_tab_rawrindex_jit(lua_State *L, const struct argbuf *ab);
#endif /* LJ_HASJIT */

/* Introspection into tables: */

struct tab_info {
  size_t acapacity; /* # of slots allocated for the array part. */
  size_t asize; /* # of non-nil slots in the array part */
  size_t hcapacity; /* # of slots allocated for the hash part. */
  size_t hsize; /* # of traversable key-value pairs in the hash part. */
  size_t hnchains; /* # of collision chains in the hash part. */
  size_t hmaxchain; /* Max. length of a collision chain. */
};

/* Stores introspection information about t to ti. */
void lj_tab_getinfo(lua_State *L, const GCtab *t, struct tab_info *ti);

#endif
