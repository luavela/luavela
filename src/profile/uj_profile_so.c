/*
 * This module is used for gathering info about loaded shared objects.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#define _GNU_SOURCE 1

#include <stdio.h>
#include <unistd.h>
#include <link.h>
#include <sys/auxv.h>

#include "profile/uj_profile_impl.h"
#include "uj_mem.h"

static int profile_so_valid(const char *name)
{
	return strstr(name, ".so") != NULL;
}

static void profile_so_add(const char *name, ElfW(Addr) addr,
			   struct shared_obj *so)
{
	if (!so)
		return;

	so->base = (uintptr_t)addr;
	strncpy(so->path, name, SO_MAX_PATH_LENGTH - 1);
	so->path[SO_MAX_PATH_LENGTH - 1] = '\0';
}

static int profile_so_count(struct dl_phdr_info *info, size_t sz, void *counter)
{
	UNUSED(sz);
	lua_assert(counter);

	if (profile_so_valid(info->dlpi_name))
		(*(size_t *)counter)++;
	return 0;
}

static void profile_so_add_binary_path(struct shared_obj *so, ElfW(Addr) reloc)
{
	char buf[SO_MAX_PATH_LENGTH] = {0};
	ssize_t bytes = readlink("/proc/self/exe", buf, SO_MAX_PATH_LENGTH);

	/* In this case profiler parser output will be w/o SO's symbols. */
	if (bytes == -1)
		return;

	buf[bytes < SO_MAX_PATH_LENGTH ? bytes : SO_MAX_PATH_LENGTH - 1] = '\0';
	profile_so_add(buf, reloc, so);
}

static int profile_so_fill(struct dl_phdr_info *info, size_t sz, void *iter_so)
{
	UNUSED(sz);
	struct shared_obj *so;
	size_t i = 0;

	lua_assert(iter_so);
	so = *(struct shared_obj **)iter_so;

	/*
	 * We are iterating through all binary objects in memory and trying to
	 * find SO's .text section to determine relocation address.
	 * NB! Current implementation works correctly if SO has only one code
	 * section.
	 */
	for (; i < info->dlpi_phnum; i++) {
		const ElfW(Addr)
			start = info->dlpi_addr + info->dlpi_phdr[i].p_vaddr;
		const ElfW(Addr) end = start + info->dlpi_phdr[i].p_memsz;
		const ElfW(Addr) pfunc = (ElfW(Addr))profile_so_fill;

		if (info->dlpi_phdr[i].p_type != PT_LOAD)
			continue;

		if (!(pfunc >= start && pfunc < end))
			continue;

		profile_so_add_binary_path(so, info->dlpi_addr);
		(*(struct shared_obj **)iter_so)++;
		return 0;
	}

	if (!profile_so_valid(info->dlpi_name))
		return 0;

	profile_so_add(info->dlpi_name, info->dlpi_addr, so);

	(*(struct shared_obj **)iter_so)++;
	return 0;
}

static void profile_so_add_vdso(struct shared_obj **iter_so)
{
	struct shared_obj *so = *(iter_so);

	/* Let's ignore return value of getauxval. */
	profile_so_add(VDSO_NAME, getauxval(AT_SYSINFO_EHDR), so);
	(*iter_so)++;
}

int uj_profile_so_init(lua_State *L, struct profiler_state *ps)
{
	struct shared_obj *iter_so;
	size_t sizeof_objects;

	ps->so = uj_mem_alloc(L, sizeof(*ps->so));
	/* 1 for path to the executable, 1 for vDSO. */
	ps->so->num = 2;

	dl_iterate_phdr(profile_so_count, &ps->so->num);
	sizeof_objects = ps->so->num * sizeof(struct shared_obj);
	ps->so->objects = uj_mem_alloc(L, sizeof_objects);

	iter_so = ps->so->objects;

	profile_so_add_vdso(&iter_so);
	dl_iterate_phdr(profile_so_fill, &iter_so);
	return 0;
}

void uj_profile_so_free(struct profiler_state *ps)
{
	if (!ps->so)
		return;

	uj_mem_free(MEM_G(ps->g), ps->so->objects,
		    ps->so->num * sizeof(struct shared_obj));
	uj_mem_free(MEM_G(ps->g), ps->so, sizeof(*(ps->so)));

	ps->so = NULL;
}

void uj_profile_so_get_rip(struct profiler_state *ps, const void *ctx)
{
	ps->context.rip =
		(uint64_t)((ucontext_t *)ctx)->uc_mcontext.gregs[REG_RIP];
}
