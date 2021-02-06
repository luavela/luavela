/*
 * Poor std::vector.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <stdlib.h>

#include "ujpp_vector.h"
#include "ujpp_utils.h"

#define DEFAULT_ELEMS_NUM 10

void ujpp_vector_init(struct vector *v)
{
	v->size = 0;
	v->capacity = DEFAULT_ELEMS_NUM;
	v->elems = ujpp_utils_allocz((sizeof(*v->elems) * v->capacity));
}

void ujpp_vector_add(struct vector *v, void *elem)
{
	size_t new_len;

	if (v->size != v->capacity) {
		v->elems[v->size++] = elem;
		return;
	}

	new_len = v->capacity * 2;
	v->elems = realloc(v->elems, new_len * sizeof(void *));

	if (NULL == v->elems) {
		ujpp_utils_die("realloc failed", NULL);
		return; /* unreachable */
	} else {
		v->elems[v->size++] = elem;
	}

	v->capacity = new_len;
}

void ujpp_vector_free(const struct vector *v)
{
	for (size_t i = 0; i < v->size; i++)
		free(v->elems[i]);
	free(v->elems);
}

void *ujpp_vector_at(struct vector *v, size_t i)
{
	size_t vec_len = ujpp_vector_size(v);

	if (i >= vec_len)
		ujpp_utils_die("Array index is out of bounds: %zu while vector"
			       " has only %zu elements",
			       i, vec_len);
	return v->elems[i];
}

size_t ujpp_vector_size(const struct vector *v)
{
	return v->size;
}
