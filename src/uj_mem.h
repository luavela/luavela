/*
 * Dynamic memory management. This module provides convenience
 * wrappers around a pluggable memory allocator.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */
#ifndef _UJ_MEM_H
#define _UJ_MEM_H

#include "lua.h"
#include "lj_def.h"
#include "utils/uj_alloc.h"

struct luae_Options;

/*
 * Metrics of the memory manager.
 * NB! "metrics flush" = last call to uj_mem_flush_metrics
 */
struct mem_metrics {
	size_t total; /* number of bytes currently allocated */
	size_t allocated; /* number of bytes allocated since metrics flush */
	size_t freed; /* number of bytes freed since metrics flush */
};

struct mem_manager {
	lua_Alloc allocf; /* allocation function */
	void *state; /* state of the allocator */
	struct mem_metrics metrics;
};

/*
 * Initializes an instance of the memory manager.
 * Returns 0 on success, and a non-0 value otherwise.
 */
int uj_mem_init(struct mem_manager *mem, const struct luae_Options *opt);

/* Terminates the instance of the memory manager. */
void uj_mem_terminate(struct mem_manager *mem);

/*
 * Allocates `size` bytes. If there is not enough memory, returns NULL without
 * throwing an exception (see below). Must be used only for initial memory
 * allocation until the very first Lua state is created.
 */
void *uj_mem_alloc_nothrow(struct mem_manager *mem, size_t size);

/*
 * Core interfaces for memory allocation/deallocation.
 * As per the Reference Manual, a single low-level allocation interface for Lua
 * states must be of type lua_Alloc (p. 3.7). Hence *_alloc is implemented via
 * *_realloc and object sizes are explicitly passed in *_realloc and *_free.
 * *_alloc and *_realloc throw if there is not enough memory to complete the
 * operation.
 */

void *uj_mem_realloc(lua_State *L, void *ptr, size_t osize, size_t nsize);

static LJ_AINLINE void *uj_mem_alloc(lua_State *L, size_t size)
{
	return uj_mem_realloc(L, NULL, 0, size);
}

static LJ_AINLINE void *uj_mem_calloc(lua_State *L, size_t size)
{
	void *res = uj_mem_alloc(L, size);

	memset(res, 0, size);
	return res;
}

static LJ_AINLINE void uj_mem_free(struct mem_manager *mem, void *ptr,
				   size_t size)
{
	mem->metrics.total -= size;
	mem->metrics.freed += size;
	lua_assert(mem->allocf != NULL);
	mem->allocf(mem->state, ptr, size, 0);
}

/* Interfaces for working with metrics. */

static LJ_AINLINE const struct mem_metrics *
uj_mem_metrics(const struct mem_manager *mem)
{
	return &(mem->metrics);
}

static LJ_AINLINE void uj_mem_flush_metrics(struct mem_manager *mem)
{
	mem->metrics.allocated = 0;
	mem->metrics.freed = 0;
}

static LJ_AINLINE size_t uj_mem_total(const struct mem_manager *mem)
{
	return mem->metrics.total;
}

/* NB! This hack is needed by sealing. */
static LJ_AINLINE void uj_mem_inc_total(struct mem_manager *mem, size_t n)
{
	mem->metrics.total += n;
}

/* NB! This hack is needed by sealing. */
static LJ_AINLINE void uj_mem_dec_total(struct mem_manager *mem, size_t n)
{
	mem->metrics.total -= n;
}

/*
 * Grow size of a vector vec with each element having a size of esize bytes.
 * Final size will not exceed maxsize bytes. The original size is read from
 * and the new size is written to psize. Throws if there is not enough memory
 * to complete the operation.
 */
void *uj_mem_grow(lua_State *L, void *vec, size_t *psize, size_t maxsize,
		  size_t esize);

#endif /* !_UJ_MEM_H */
