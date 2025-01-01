/*
 * Implementation of the dynamic memory management.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "lua.h"
#include "lextlib.h"
#include "lj_obj.h"
#include "uj_throw.h"
#include "uj_errmsg.h"
#include "uj_mem.h"
#include "uj_state.h"
#include "lj_vm.h"

/* Throws an out-of-memory error. */
static void mem_err(lua_State *L)
{
	/* Don't touch the stack during lua_open. */
	if (uj_state_has_event(L, EXTEV_VM_INIT))
		lj_vm_unwind_c(L->cframe, LUA_ERRMEM);

	uj_state_stack_sync_top(L);
	setstrV(L, L->top++, uj_errmsg_str(L, UJ_ERR_ERRMEM));
	uj_throw(L, LUA_ERRMEM);
}

/* Returns 1 if mem_manager state ownership is implemented inside uJIT */
static LJ_AINLINE int mem_is_builtin_alloc(const struct mem_manager *mem)
{
	return mem->allocf == uj_alloc_f;
}

int uj_mem_init(struct mem_manager *mem, const struct luae_Options *opt)
{
	lua_Alloc allocf = opt != NULL ? opt->allocf : NULL;
	void *state = opt != NULL ? opt->allocud : NULL;

	mem->allocf = allocf != NULL ? allocf : uj_alloc_f;
	if (mem_is_builtin_alloc(mem)) {
		if (uj_alloc_create(&(mem->state)) != 0)
			return 1;
	} else {
		mem->state = state;
	}
	memset(&(mem->metrics), 0, sizeof(mem->metrics));
	return 0;
}

void uj_mem_terminate(struct mem_manager *mem)
{
	if (mem_is_builtin_alloc(mem))
		uj_alloc_destroy(mem->state);
}

void *uj_mem_alloc_nothrow(struct mem_manager *mem, size_t size)
{
	void *ptr;

	lua_assert(mem->allocf != NULL);
	ptr = mem->allocf(mem->state, NULL, 0, size);
	if (NULL == ptr && size > 0)
		return NULL;

	lua_assert((0 == size) == (NULL == ptr));
	mem->metrics.total += size;
	mem->metrics.allocated += size;

	return ptr;
}

void *uj_mem_realloc(lua_State *L, void *ptr, size_t osize, size_t nsize)
{
	struct mem_manager *mem = MEM(L);
	lua_assert((0 == osize) == (NULL == ptr));
	lua_assert(mem->allocf != NULL);

	G(L)->L_mem = L;
	ptr = mem->allocf(mem->state, ptr, osize, nsize);
	if (NULL == ptr && nsize > 0)
		mem_err(L);

	lua_assert((0 == nsize) == (NULL == ptr));
	mem->metrics.total = (mem->metrics.total - osize) + nsize;
	mem->metrics.allocated += nsize;
	mem->metrics.freed += osize;

	return ptr;
}

/* Returns a new size (in number of elements) for a growable vector. */
static size_t mem_grow_nsize(size_t osize, size_t maxsize)
{
	size_t nsize = osize << 1;
	if (nsize < LJ_MIN_VECSZ)
		nsize = LJ_MIN_VECSZ;
	if (nsize > maxsize)
		nsize = maxsize;
	return nsize;
}

void *uj_mem_grow(lua_State *L, void *vec, size_t *psize, size_t maxsize,
		  size_t esize)
{
	size_t osize = *psize;
	size_t nsize = mem_grow_nsize(osize, maxsize);
	lua_assert(nsize > osize && nsize <= maxsize);
	vec = uj_mem_realloc(L, vec, osize * esize, nsize * esize);
	lua_assert(NULL != vec);
	*psize = nsize;
	return vec;
}
