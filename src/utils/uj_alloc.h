/*
 * Interface between uJIT and an arbitrary memory allocator.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJ_ALLOC_H
#define _UJ_ALLOC_H

#include "lj_def.h"

/*
 * Memory-allocation function used by Lua states. Must be of lua_Alloc type
 * (see lua.h). See the Reference Manual regarding function's behaviour and
 * return value.
 */
void *uj_alloc_f(void *ud, void *ptr, size_t osize, size_t nsize);

/*
 * If an allocator stores its state explicitly, this function must be used to
 * initialize it and store a pointer to the state to pud.
 * Returns 0 on success, and a non-0 value otherwise.
 */
int uj_alloc_create(void **pud);

/*
 * If an allocator stores its state explicitly, this function must be used to
 * destroy it.
 * Returns 0 on success, and a non-0 value otherwise.
 */
int uj_alloc_destroy(void *ud);

struct alloc_stats {
	/*
	 * Total number of bytes in active pages allocated
	 * by the application. May or may not include space
	 * entirely devoted to allocator metadata.
	 */
	size_t active;

	/* If available; number of bytes allocated by the application. */
	size_t allocated;

	/*
	 * If available; maximum number of bytes in physically
	 * resident data pages mapped by the allocator,
	 * comprising all kinds of overhead.
	 */
	size_t resident;

	/*
	 * If available; total number of bytes in virtual memory mappings that
	 * were retained rather than being returned to the operating system.
	 */
	size_t retained;
};

/* Returns internal statistics from the allocator. */
struct alloc_stats uj_alloc_stats(void);

#endif /* !_UJ_ALLOC_H */
