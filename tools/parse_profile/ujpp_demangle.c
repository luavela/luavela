/*
 * This module provides functionality to demangle dumped address using symbol
 * tables.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <libgen.h>
#include <sys/stat.h>
#include <sys/auxv.h>

#include "ujpp_main.h"
#include "ujpp_utils.h"
#include "ujpp_elf.h"

#define TLB_SIZE 0x400
#define TLB_TRUNC (TLB_SIZE - 1)
#define TLB_MISS (char *)(-1)

/* See torvalds/linux/blob/master/tools/testing/selftests/x86/test_vdso.c */
#define MAPS_LINE_LEN 128

struct tlb_entry {
	uint64_t addr;
	size_t so_idx;
	size_t sym_idx;
};

int ujpp_demangle_valid(const struct hvmstate *hvmst)
{
	return hvmst->di.func_addr != -1 && hvmst->symbol;
}

void ujpp_demangle(const struct parser_state *ps)
{
	for (size_t i = 0; i < ps->vec_cfunc_cache.size; i++) {
		struct cfunc_cache *cache = ps->vec_cfunc_cache.elems[i];

		cache->symbol = ujpp_demangle_getinfo(&ps->vec_loaded_so,
						      cache->addr, NULL);
	}
}

static int demangle_cmp_syms(const void *sym_first, const void *sym_second)
{
	const struct sym_info *si1 = *(const struct sym_info **)sym_first;
	const struct sym_info *si2 = *(const struct sym_info **)sym_second;

	return (int)((int64_t)si1->addr - (int64_t)si2->addr);
}

static size_t demangle_determine_vdso_size(void)
{
	FILE *fp;
	char str[MAPS_LINE_LEN];

	fp = fopen("/proc/self/maps", "r");

	if (fp == NULL)
		return 0;

	while (fgets(str, sizeof(str), fp) != NULL) {
		char *delim;
		void *start;
		void *end;

		if (!strstr(str, VDSO_NAME))
			continue;

		delim = strstr(str, "-");

		if (delim == NULL) {
			fclose(fp);
			return 0;
		}

		sscanf(str, "%p", &start);
		sscanf(delim + 1, "%p", &end);

		fclose(fp);
		return (size_t)((uint8_t *)end - (uint8_t *)start);
	}

	fclose(fp);
	return 0;
}

void ujpp_demangle_load_so(struct vector *loaded_so, const char *path,
			   uint64_t base, enum so_type type)
{
	struct shared_obj *so;
	struct stat st;
	int found = stat(path, &st) != -1;

	so = ujpp_utils_allocz(sizeof(*so));
	ujpp_vector_init(&so->symbols);

	so->path = path;
	so->base = base;
	so->short_name = basename((char *)so->path);
	so->found = found != 0;

	switch (type) {
	case SO_BIN: {
		so->size = ujpp_elf_text_sz(so->path);

		if (so->found)
			ujpp_elf_parse_file(so->path, &so->symbols);
		break;
	}
	case SO_VDSO: {
		const char *vdso = (const char *)getauxval(AT_SYSINFO_EHDR);
		size_t vdso_sz = demangle_determine_vdso_size();

		/* getauxval() fails under valgrind for example. */
		if (vdso == NULL || vdso_sz == 0)
			break;

		so->size = vdso_sz;
		so->found = 1;
		ujpp_elf_parse_mem(vdso, &so->symbols);
		break;
	}
	case SO_SHARED: {
		if (found) {
			so->size = st.st_size;
			ujpp_elf_parse_file(so->path, &so->symbols);
		}
		break;
	}
	default:
		ujpp_utils_die("Wrong object type %u", type);
	}

	if (so->found)
		qsort(so->symbols.elems, so->symbols.size, sizeof(void *),
		      demangle_cmp_syms);
	ujpp_vector_add(loaded_so, so);
}

static const char *demangle_findsym_slow(const struct shared_obj *so,
					 uint64_t addr,
					 struct demangle_info *di)
{
	const struct sym_info *si;
	size_t i = 0;

	for (; i < so->symbols.size; i++) {
		uint64_t start;
		uint64_t end;
		si = so->symbols.elems[i];

		start = so->base + si->addr;
		end = so->base + si->addr + si->size;

		if (addr >= start && addr < end) {
			di->func_addr = si->addr;
			di->sym_idx = i;

			return si->name;
		}
	}

	return NULL;
}

static const char *demangle_findsym_fast(size_t l, size_t r, uint64_t addr,
					 const struct shared_obj *so,
					 struct demangle_info *di)
{
	struct sym_info *si;
	size_t mid = l + (r - l) / 2;
	uint64_t start;
	uint64_t end;

	if (!so->symbols.size)
		return NULL;

	if (l > r)
		return NULL;

	si = so->symbols.elems[mid];

	start = so->base + si->addr;
	end = so->base + si->addr + si->size;

	if (addr >= start && addr < end) {
		di->func_addr = si->addr;
		di->sym_idx = mid;

		return si->name;
	}

	if (mid == 0)
		return NULL;

	if (start > addr)
		return demangle_findsym_fast(l, mid - 1, addr, so, di);

	return demangle_findsym_fast(mid + 1, r, addr, so, di);
}

static int demangle_addr_belongs_so(const struct shared_obj *so, uint64_t addr)
{
	/* File not found */
	if (!so->found)
		return 0;

	if (!(addr >= so->base && addr < (so->base + so->size)))
		return 0;
	return 1;
}
static const char *demangle_check_tlb(const struct tlb_entry *tlb,
				      const struct vector *vec_so,
				      uint64_t addr, struct demangle_info *di)
{
	const struct tlb_entry *entry = &tlb[addr & TLB_TRUNC];
	const struct shared_obj *so;
	const struct sym_info *si;

	/* Not uninitialized. */
	if (!entry->addr)
		return TLB_MISS;

	/* Initialized but filled with another symbol. */
	if (addr != entry->addr)
		return TLB_MISS;

	di->so_idx = entry->so_idx;
	di->sym_idx = entry->sym_idx;

	if (entry->so_idx == -1)
		return NULL;

	if (entry->sym_idx == -1)
		return NULL;

	so = vec_so->elems[entry->so_idx];
	si = so->symbols.elems[entry->sym_idx];
	di->func_addr = si->addr;

	return si->name;
}

static void demangle_update_tlb(struct tlb_entry *tlb, uint64_t addr,
				size_t so_idx, size_t sym_idx)
{
	struct tlb_entry *entry = &tlb[addr & TLB_TRUNC];

	entry->addr = addr;
	entry->so_idx = so_idx;
	entry->sym_idx = sym_idx;
}

const char *ujpp_demangle_getinfo(const struct vector *vec_loaded_so,
				  uint64_t addr, struct demangle_info *di)
{
	static struct tlb_entry tlb[TLB_SIZE]; /* ~90% hitrate. */
	const struct shared_obj *so = NULL;
	struct demangle_info local_di;
	const char *name;

	if (!di)
		di = &local_di;

	di->so_idx = -1;
	di->sym_idx = -1;
	di->func_addr = -1;

	name = demangle_check_tlb(tlb, vec_loaded_so, addr, di);

	if (name != TLB_MISS)
		return name;

	for (size_t i = 0; i < vec_loaded_so->size; i++) {
		so = vec_loaded_so->elems[i];

		if (demangle_addr_belongs_so(so, addr)) {
			di->so_idx = i;
			break;
		}
	}

	if (di->so_idx == -1 || so == NULL) {
		demangle_update_tlb(tlb, addr, di->so_idx, di->sym_idx);
		return NULL;
	}

	name = demangle_findsym_fast(0, so->symbols.size - 1, addr, so, di);

	/*
	 * Binary search may fail when some function inlined in the body of
	 * other function. Then just restart with linear search.
	 */
	if (!name)
		name = demangle_findsym_slow(so, addr, di);

	demangle_update_tlb(tlb, addr, di->so_idx, di->sym_idx);

	return name;
}

void ujpp_demangle_free_symtab(struct shared_obj *so)
{
	for (size_t i = 0; i < so->symbols.size; i++) {
		const struct sym_info *si = so->symbols.elems[i];

		free((void *)si->name);
	}

	ujpp_vector_free(&so->symbols);
}
