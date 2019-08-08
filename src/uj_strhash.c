/*
 * Implementation of a non-owning string hash.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "lj_obj.h"
#include "lj_gc.h"
#include "uj_mem.h"
#include "uj_strhash.h"

#ifndef NDEBUG
#include <stdio.h>
#include "uj_str.h"
#endif /* !NDEBUG */

static LJ_AINLINE size_t strhash_index(uint32_t hash, size_t mask)
{
	return hash & mask;
}

static LJ_AINLINE void strhash_destroy_hash(uj_strhash_t *strhash,
					    global_State *g)
{
	uj_mem_free(MEM_G(g), strhash->hash,
		    (strhash->mask + 1) * sizeof(GCobj *));
}

static void strhash_resize(uj_strhash_t *strhash, lua_State *L, size_t newmask)
{
	global_State *g = G(L);
	GCobj **newhash;
	size_t i;
	const size_t newhsize = (newmask + 1) * sizeof(GCobj *);

	/* No resizing during GC traversal or if already too big. */
	if (g->gc.state == GCSsweepstring || newmask >= LJ_MAX_STRTAB - 1)
		return;

	newhash = uj_mem_calloc(L, newhsize);
	for (i = strhash->mask; i != ~(size_t)0; i--) { /* Rehash old table. */
		GCobj *p = strhash->hash[i];

		/* Follow each hash chain and reinsert all strings. */
		while (p) {
			size_t j = strhash_index(gco2str(p)->hash, newmask);
			GCobj *next = gcnext(p);

			/* NOBARRIER: The string table is a GC root. */
			p->gch.nextgc = newhash[j];
			newhash[j] = p;
			p = next;
		}
	}
	strhash_destroy_hash(strhash, g);
	strhash->mask = newmask;
	strhash->hash = newhash;
}

int uj_strhash_shrink(uj_strhash_t *strhash, lua_State *L)
{
	size_t hmask = strhash->mask;

	if (strhash->count > (hmask >> 2) || hmask <= LJ_MIN_STRTAB * 2 - 1)
		return 0;

	strhash_resize(strhash, L, strhash->mask >> 1);
	return 1;
}

static GCstr *strhash_find_in_bucket(GCobj *o, const char *str, size_t len)
{
	for (; o != NULL; o = gcnext(o)) {
		GCstr *sx = gco2str(o);

		if (sx->len == len && memcmp(str, strdata(sx), len) == 0)
			return sx; /* Return existing string. */
	}
	return NULL;
}

GCstr *uj_strhash_find(const uj_strhash_t *strhash, const char *str, size_t len,
		       uint32_t hash)
{
	GCobj *o = strhash->hash[strhash_index(hash, strhash->mask)];

	lua_assert(str != NULL);
	return strhash_find_in_bucket(o, str, len);
}

void uj_strhash_add(uj_strhash_t *strhash, lua_State *L, GCstr *s)
{
	size_t i;

	lua_assert(s != NULL);
	i = strhash_index(s->hash, strhash->mask);
	s->nextgc = strhash->hash[i];
	/* NOBARRIER: The string table is a GC root. */
	strhash->hash[i] = obj2gco(s);

	if (strhash->count++ > strhash->mask) /* Allow a 100% load factor. */
		strhash_resize(strhash, L, (strhash->mask << 1) + 1);
}

void uj_strhash_destroy(uj_strhash_t *strhash, global_State *g)
{
	lua_assert(0 == strhash->count);
	strhash_destroy_hash(strhash, g);
}

#ifndef NDEBUG
static int strhash_check_sealed(const uj_strhash_t *strhash,
				const uj_strhash_t *strhash_sealed)
{
	size_t i;
	GCobj *o;

	/* Regular strhash must not contain sealed strings... */
	for (i = 0; i <= strhash->mask; i++)
		for (o = strhash->hash[i]; o != NULL; o = gcnext(o))
			if (uj_obj_is_sealed(o))
				return 1;

	/* ... and vice versa. */
	for (i = 0; i <= strhash_sealed->mask; i++)
		for (o = strhash_sealed->hash[i]; o != NULL; o = gcnext(o))
			if (!uj_obj_is_sealed(o))
				return 1;

	return 0;
}
#endif /* !NDEBUG */

static size_t strhash_relink_bucket(GCobj **it, uj_strhash_t *strhash_sealed,
				    lua_State *L)
{
	GCobj *o = NULL;
	size_t evicted = 0;

	lua_assert(!gl_datastate(G(L)));
	while ((o = *it) != NULL) {
		if (uj_obj_is_sealed(o)) {
			/* Evict sealed object from regular strhash... */
			*it = gcnext(o);
			evicted++;

			/* ... and add it to the sealed strhash. */
			uj_strhash_add(strhash_sealed, L, gco2str(o));
		} else {
			it = &gcnext(o);
		}
	}
	return evicted;
}

void uj_strhash_relink(uj_strhash_t *strhash, uj_strhash_t *strhash_sealed,
		       lua_State *L)
{
	size_t i;
	size_t evicted;

	lua_assert(G(L)->gc.state == GCSpause);
	for (i = 0; i <= strhash->mask; i++) {
		GCobj **bucket = &strhash->hash[i];

		evicted = strhash_relink_bucket(bucket, strhash_sealed, L);
		lua_assert(evicted < strhash->count);
		strhash->count -= evicted;
	}

	lua_assert(0 == strhash_check_sealed(strhash, strhash_sealed));
}

void uj_strhash_init(uj_strhash_t *strhash, lua_State *L)
{
	lua_assert(~(size_t)0 == strhash->mask);
	strhash_resize(strhash, L, LJ_MIN_STRTAB - 1);
}

#ifndef NDEBUG
/*
 * This diagnostic function returns total amount of memory,
 * consumed by the contained strings. Not static, to avoid
 * compiler warnings, but no public declaration as well. Use
 * under GDB for your convenience, don't tell anyone.
 */
size_t uj_strhash_memcount(const uj_strhash_t *strhash)
{
	size_t mem = 0;
	size_t i;

	for (i = 0; i <= strhash->mask; i++)
		for (GCobj *o = strhash->hash[i]; o != NULL; o = gcnext(o))
			mem += uj_str_sizeof(gco2str(o));

	return mem;
}
/*
 * This diagnostic function prints number of buckets of particular sizes,
 * starting with 0 (empty bucket) and up to some maximum value N
 * (which in fact means "bucket at least N objects long").
 * Not static, to avoid compiler warnings, but no public declaration as well.
 * Use under GDB for your convenience, don't tell anyone.
 */
#define NUM_BUCKET_CLASSES 10
void uj_strhash_countbuckets(const uj_strhash_t *strhash)
{
	size_t sizes[NUM_BUCKET_CLASSES] = {0};
	size_t i;

	for (i = 0; i <= strhash->mask; i++) {
		size_t size = 0;

		for (GCobj *o = strhash->hash[i]; o != NULL; o = gcnext(o))
			size++;

		if (size >= NUM_BUCKET_CLASSES)
			size = NUM_BUCKET_CLASSES - 1;

		sizes[size]++;
	}

	for (i = 0; i < NUM_BUCKET_CLASSES; i++)
		fprintf(stdout, "%zd = %zd\n", i, sizes[i]);
}
#endif /* !NDEBUG */
