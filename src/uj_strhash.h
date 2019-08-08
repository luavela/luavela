/*
 * Non-owning string hash.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJ_STRHASH_H
#define _UJ_STRHASH_H

#include "lj_obj.h"

/*
 * String hash is a collection of unique GCstr objects. This means
 * that, when properly used, all contained objects will have different
 * string payload.
 * String hash does not retain any ownership of actual string objects.
 * This means that the object should be constructed prior to insertion
 * in the hash and somehow managed after eviction from the hash. Also,
 * uj_strhash_destroy must only be called on empty hashes.
 * The general workflow is as follows:
 * 1) A struct of type uj_strhash_t is allocated.
 * 2) "mask" field of the struct is set to ~0 (see uj_state.c).
 * 3) String interning is done via uj_strhash_find and uj_strhash_add.
 *    Note that uj_strhash_add itself does not check object uniqueness.
 *    One must check that string is not present in the hash and only call
 *    uj_strhash_add afterwards.
 * 4) The most common scenario of evicting a string from the hash is inside
 *    the garbage collector. GC operates on hash internals directly and
 *    manages evicted objects itself.
 * 5) One special scenario is evicting sealed objects from regular string hash
 *    and moving them to sealed string hash. Corresponding function is present.
 * 6) Hash shrinking is available if too many strings were evicted from it.
 *    This allows to spare some memory.
 * 7) As the hash itself allocates some memory, it must be properly destroyed.
 *    To avoid memory leaks, the hash must only be destroyed when all strings
 *    are already evicted from it.
 */

/*
 * Initialize already allocated strhash in context of lua_State L.
 * Strhash might have arbitrary values of fields, except mask must be ~0 -
 * this is an implementation requirement.
 * This function must be called on a structure prior to calling any other
 * interface methods on the same structure.
 * Returns void, but throws in case of memory allocation error.
 */
void uj_strhash_init(uj_strhash_t *strhash, lua_State *L);

/*
 * Try to find a string with particular payload in the hash.
 * 'hash' param is the exact hash of the passed string. It might as well be
 * calculated inside the function, but due to the design it is already computed
 * by the time this function is called.
 * Returns NULL if string is not found or non-NULL string pointer otherwise.
 */
GCstr *uj_strhash_find(const uj_strhash_t *strhash, const char *str,
		       size_t lenx, uint32_t hash);

/*
 * Add string to the hash.
 * This function does not check string uniqueness for the hash, so
 * uj_strhash_find must be called prior to this.
 * Returns void, but throws in case of memory allocation error due to hash
 * growth.
 */
void uj_strhash_add(uj_strhash_t *strhash, lua_State *L, GCstr *s);

/*
 * Evict all sealed strings from strhash and add them to strhash_sealed.
 * Returns void, but throws in case of memory allocation error due to hash
 * growth or shrink.
 */
void uj_strhash_relink(uj_strhash_t *strhash, uj_strhash_t *strhash_sealed,
		       lua_State *L);

/*
 * Attempts to spare some memory, when number of strings in hash is
 * significantly less than hash capacity.
 * Returns 1 in case of successful shrink, 0 otherwise. Throws in case of memory
 * allocation error.
 */
int uj_strhash_shrink(uj_strhash_t *strhash, lua_State *L);

/*
 * Free dynamically allocated hash data structures.
 * Must only be called when all strings were already evicted from the hash.
 */
void uj_strhash_destroy(uj_strhash_t *strhash, global_State *g);

#endif /* !_UJ_STRHASH_H */
