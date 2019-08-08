/*
 * Hash table interfaces.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJPP_HASHTABLE_H
#define _UJPP_HASHTABLE_H

#include <inttypes.h>

/* Number of elements in the chaining hash table */
#define HASH_TABLE_SIZE 50000

struct hash {
	struct vector **arr;
};

/* Avalanche fast hash function*/
unsigned int ujpp_hash_avalanche(unsigned int x);
/* Allocates memory for internal array */
void ujpp_hash_init(struct hash *h);
/* Free mem using callback function */
void ujpp_hash_free(const struct hash *h, void (*callback)(void *));
/* Gather all elements from the table and pushes them in vector */
void ujpp_hash_2_vector(const struct hash *t, struct vector *out);
/* Adds element to table */
void *ujpp_hash_insert(struct hash *h, uint32_t hash, void *elem,
		       int (*cmpfunc)(const void *, const void *));

#endif /* !_UJPP_HASHTABLE_H */
