/*
 * Interfaces for dynamic-size array.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJPP_VECTOR_H
#define _UJPP_VECTOR_H

#include <stdio.h>

struct vector {
	void **elems;
	size_t size;
	size_t capacity;
};

/* Allocates memory for vector */
void ujpp_vector_init(struct vector *v);
/* Adds one element to the end of array */
void ujpp_vector_add(struct vector *v, void *elem);
/* Free memory used by the vector */
void ujpp_vector_free(const struct vector *v);
void *ujpp_vector_at(struct vector *v, size_t i);
size_t ujpp_vector_size(const struct vector *v);

#endif /* !_UJPP_VECTOR_H */
