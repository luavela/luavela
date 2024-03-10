/*
 * Integration of jemalloc (http://jemalloc.net) with uJIT.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef NDEBUG
#include <stdio.h>
#endif /* !NDEBUG */

#include <unistd.h>
#include <sys/mman.h>

#include "jemalloc/jemalloc.h"
#include "utils/uj_alloc.h"

/*
 * Non-default jemalloc options in the option string below.
 *
 * abort_conf:true   - Abort on invalid option string.
 * narenas:1         - Limit the number of automatically created allocation
 *                     arenas to 1 because we manage them manually.
 * tcache:false      - Disable per-thread cache. In our scenario we use one
 *                     allocation arena per VM instance, and multiple threads
 *                     never compete for the access to the same arena.
 *
 * If you decide to reduce memory footprint, consider following options,
 * experiments showed that they do not degrade performance:
 *
 * retain:false      - do not retain virtual memory returning it to the OS.
 *                     Although not recommended for 64-bit Linux in general,
 *                     disabling this looked fine for our use cases.
 *
 * Other use cases.
 *
 * To check for leaks, add following options to the option string:
 * prof_leak:true,prof_final:true,lg_prof_sample:0,prof_prefix:/tmp/jeprof
 * (if your application crashes, increase lg_prof_sample).
 *
 * To profile allocations, add following options to the option string:
 * prof:true,lg_prof_interval:25,lg_prof_sample:19,prof_prefix:/tmp/jeprof
 *
 * Please note that jemalloc must be configured with --enable-prof to use any
 * profiling / memory leak detection features.
 *
 * References
 * 1. https://github.com/jemalloc/jemalloc/issues/1098 -
 *    A discussion with jemalloc developers on setting non-default options.
 * 2. http://jemalloc.net/jemalloc.3.html -
 *    A full list of jemalloc options.
 */
const char *jem_malloc_conf = "abort_conf:true,narenas:1,tcache:false";

struct jem_state {
	size_t size;           /* size mmap'ed for this struct */
	unsigned int arena_id; /* arena used for this VM instance */
	int flags;             /* extended allocation flags */
};

/* Just in case sysconf fails, who knows? */
#define DEFAULT_PAGESIZE ((long)4096)

void *uj_alloc_f(void *ud, void *ptr, size_t osize, size_t nsize)
{
	UNUSED(osize);
	struct jem_state *state = (struct jem_state *)ud;

	if (0 == nsize) {
		if (ptr != NULL)
			jem_dallocx(ptr, state->flags);
		return NULL;
	}

	if (NULL == ptr)
		return jem_mallocx(nsize, state->flags);

	return jem_rallocx(ptr, nsize, state->flags);
}

int uj_alloc_create(void **pud)
{
	struct jem_state *state;
	long pg_size;
	unsigned int arena_id;
	size_t arena_id_size = sizeof(arena_id);
	int status;

	pg_size = sysconf(_SC_PAGESIZE);
	if (-1 == pg_size)
		pg_size = DEFAULT_PAGESIZE;

	lua_assert(pg_size >= 1);

	state = mmap(NULL, (size_t)pg_size,
		     PROT_READ | PROT_WRITE,
		     MAP_PRIVATE | MAP_ANONYMOUS,
		     -1, 0);
	if (state == NULL)
		return 1;

	status = jem_mallctl("arenas.create",
			     (void *)&arena_id, &arena_id_size,
			     NULL, 0);
	if (status != 0) {
		/*
		 * Skip munmap's return value during handling the jemalloc's
		 * error: Reporting mallctl status is more relevant anyway.
		 */
		munmap(state, (size_t)pg_size);
		return status;
	}

	state->size = (size_t)pg_size;
	state->arena_id = arena_id;
	/*
	 * We have disabled per-thread caching in the config variable (tcache,
	 * see above), but let's prohibit it via flags as well to be 100% sure:
	 */
	state->flags = MALLOCX_ARENA(arena_id) | MALLOCX_TCACHE_NONE;
	*pud = state;
	return 0;
}

static int alloc_destroy_arena(const struct jem_state *state)
{
	/* mib = Management Information Base, see jemalloc's man page. */
	size_t mib[3] = {0};
	size_t miblen = sizeof(mib) / sizeof(mib[0]);
	int status;

	status = jem_mallctlnametomib("arena.0.destroy", mib, &miblen);
	if (status != 0)
		return status;

	mib[1] = (size_t)(state->arena_id);
	status = jem_mallctlbymib(mib, miblen, NULL, NULL, NULL, 0);
	if (status != 0)
		return status;

	return 0;
}

int uj_alloc_destroy(void *ud)
{
	struct jem_state *state = (struct jem_state *)ud;
	int status;

	status = alloc_destroy_arena(state);
	if (status != 0) {
		/*
		 * Skip munmap's return value during handling the jemalloc's
		 * error: Reporting mallctl status is more relevant anyway.
		 */
		munmap(state, state->size);
		return status;
	}

	status = munmap(state, state->size);
	if (status != 0)
		return status;

	return 0;
}

struct alloc_stats uj_alloc_stats(void)
{
	struct alloc_stats stats = {0};
	size_t sz_sz = sizeof(size_t);
	size_t sz_64 = sizeof(uint64_t);
	uint64_t epoch = 1;

	/* Refresh allocator's internal stats sources prior to reporting: */
	jem_mallctl("epoch", &epoch, &sz_64, &epoch, sz_64);

	jem_mallctl("stats.active", &stats.active, &sz_sz, NULL, 0);
	jem_mallctl("stats.resident", &stats.resident, &sz_sz, NULL, 0);
	jem_mallctl("stats.allocated", &stats.allocated, &sz_sz, NULL, 0);
	jem_mallctl("stats.retained", &stats.retained, &sz_sz, NULL, 0);

	return stats;
}

#ifndef NDEBUG
static void alloc_stats_write(void *ptr, const char *data)
{
	fprintf((FILE *)ptr, "%s", data);
}

/*
 * This diagnostic function returns verbose stats from the allocator.
 * Not static, to avoid compiler warnings, but no public declaration as well.
 * Use under debugger for your convenience, don't tell anyone.
 */
void uj_alloc_stats_print(const char *fname)
{
	FILE *file;

	if (NULL == fname)
		fname = "/tmp/ujit-jemalloc.stats";

	file = fopen(fname, "w");

	if (NULL == file) {
		fprintf(stderr, "Unable to dump to \"%s\"\n", fname);
		return;
	}

	jem_malloc_stats_print(alloc_stats_write, file, NULL);
	fclose(file);
	fprintf(stderr, "Dumped to \"%s\"\n", fname);
}
#endif /* !NDEBUG */
