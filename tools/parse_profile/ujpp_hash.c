/*
 * This module implements chained hash table.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <stdlib.h>
#include <string.h>

#include "ujpp_hash.h"
#include "ujpp_vector.h"
#include "ujpp_utils.h"

void ujpp_hash_init(struct hash *h)
{
	h->arr = ujpp_utils_allocz(HASH_TABLE_SIZE * sizeof(*h->arr));
	memset(h->arr, 0, HASH_TABLE_SIZE * sizeof(void *));
}

void ujpp_hash_free(const struct hash *h, void (*callback)(void *))
{
	for (size_t i = 0; i < HASH_TABLE_SIZE; i++) {
		struct vector *vec;

		vec = h->arr[i];

		if (NULL == vec)
			continue;

		if (NULL != callback)
			for (size_t j = 0; j < vec->size; j++)
				callback(vec->elems[j]);

		ujpp_vector_free(vec);
		free(vec);
	}

	free(h->arr);
}

void *ujpp_hash_insert(struct hash *h, uint32_t hash, void *elem,
		       int (*cmpfunc)(const void *, const void *))
{
	size_t i = hash % HASH_TABLE_SIZE;

	if (NULL == h->arr[i]) {
		struct vector *v = ujpp_utils_allocz(sizeof(*v));

		ujpp_vector_init(v);
		ujpp_vector_add(v, elem);
		h->arr[i] = v;
		return NULL;
	}

	for (size_t j = 0; j < h->arr[i]->size; j++)
		if (!(cmpfunc)(elem, h->arr[i]->elems[j]))
			return h->arr[i]->elems[j];

	ujpp_vector_add(h->arr[i], elem);
	return NULL;
}

unsigned int ujpp_hash_avalanche(unsigned int x)
{
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = (x >> 16) ^ x;
	return x;
}

void ujpp_hash_2_vector(const struct hash *t, struct vector *out)
{
	for (size_t i = 0; i < HASH_TABLE_SIZE; i++) {
		if (NULL == t->arr[i])
			continue;

		for (size_t j = 0; j < t->arr[i]->size; j++)
			ujpp_vector_add(out, t->arr[i]->elems[j]);
	}
}
