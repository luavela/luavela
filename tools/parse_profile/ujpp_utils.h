/*
 * Auxiliary parser interfaces.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJPP_UTILS_H
#define _UJPP_UTILS_H

#include <inttypes.h>
#include <stdio.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdlib.h>

struct parser_state;

enum so_type {
	SO_BIN, /* Executable file */
	SO_VDSO, /* "Fake" vDSO entry. */
	SO_SHARED, /* Shared library (.so file). */
};

/* Microseconds from epoch to YY:MM:DD MM:SS */
void ujpp_utils_print_date(const uint64_t msecs);
size_t ujpp_utils_vmst_id(const struct parser_state *ps, const size_t type);
const char *ujpp_utils_vmst_name(const size_t id);
const char *ujpp_utils_ffunc_name(const size_t id);

/* Load symbol table for dumped shared obejcts */
void ujpp_utils_read_so(struct parser_state *ps, uint64_t so_num);
/* Put messgae to stderr and exit */
void ujpp_utils_die_nofunc(const char *fmt, ...);
/* Returns vmstate name by it counter */
const char *ujpp_utils_vmst_counter_name(struct parser_state *ps,
					 size_t counter);

#define ujpp_utils_die(fmt, ...) \
	(ujpp_utils_die_nofunc("Error in %s: " fmt, __func__, __VA_ARGS__))

/* Comapre callbacks for qsort */
int ujpp_utils_cmp_vmstate(const void *first, const void *second);
int ujpp_utils_cmp_stack(const void *first, const void *second);
int ujpp_utils_cmp_trace(const void *first, const void *second);

/* Sort samples vie counters */
void ujpp_utils_sort_items(struct parser_state *ps);

/* Allocate zeroed memory */
void *ujpp_utils_allocz(size_t sz);
/* Maps file to the memory. */
char *ujpp_utils_map_file(const char *path, size_t *fsize);
/* Unmaps file. */
int ujpp_utils_unmap_file(void *mem, size_t sz);

#endif /* !_UJPP_UTILS_H */
