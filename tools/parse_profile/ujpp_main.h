/*
 * uJIT profiler parser.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJPP_MAIN_H
#define _UJPP_MAIN_H

#include <inttypes.h>

#include "ujpp_vector.h"
#include "ujpp_hash.h"
#include "ujpp_table.h"
#include "ujpp_demangle.h"
#include "../../src/uj_vmstate.h"
#include "../../src/profile/uj_profile_iface.h"

#define UJPP_STATIC_ASSERT(cond) \
	extern void ujpp(int static_assert_failed[(cond) ? 1 : -1])

struct cfunc {
	uint64_t count; /* Top frame counter */
	uint64_t cache_id; /* ID in cache_cfunc */
};

struct hvmstate {
	uint64_t count; /* Top frame counter */
	uint64_t addr; /* VA (dumped RIP) */
	const char *symbol; /* Demangled symbol */
	struct demangle_info di; /* Hold info about SO */
};

struct cfunc_cache {
	uint64_t addr; /* function address */
	uint64_t flushed; /* Flag shows that symbol was flushed to callgraph */
	const char *symbol; /* Demangled symbol */
};

struct lfunc {
	uint64_t count; /* Top frame counter */
	uint64_t cache_id; /* ID in lfunc_cache */
};

struct ffunc {
	uint64_t count; /* Top frame counter */
	uint64_t cache_id; /* ID in ffunc_cache */
};

struct ffunc_cache {
	uint64_t ffid; /* ffunc ffid */
	uint64_t flushed; /* Internal flushed ID for callgraph */
};

struct trace {
	uint64_t count; /* Top frame counter */
	char *name; /* Trace name */
	uint64_t traceno; /* Trace number */
	uint64_t line; /* Lua code line */
	uint64_t generation; /* Trace generation */
};

struct lfunc_cache {
	uint64_t addr; /* Lua chunk addr */
	uint64_t flushed; /* Internal ID for callgraph */
	char *sym; /* Symbol (path to Lua file) */
	uint64_t line; /* LOC in file */
	const char *demangled_sym; /* Lua function name */
};

struct frame_header {
	uint8_t frame_header;
	uint8_t frame_type;
	uint64_t frame_id;
};

struct stack {
	uint64_t calls; /* Number of calls */
	struct vector *v; /* Vector holds struct frame * chain */
	unsigned int hash; /* Hashnum for fast search in hashtable */
};

struct frame {
	uint8_t type; /* Frame type: cfunc, ffunc, lfunc, etc */
	uint64_t cache_id; /* ID in cache: cache_cfunc, cache_ffunc, etc */
};

struct vmst_info {
	size_t count;
	size_t type;
};

/*
 * Holds data required to report parsed VM state for which only shared
 * object info with its counters is printed.
 */
struct hvmstate_info {
	struct vector vec_rip; /* Vector of dumped RIP-s */
	struct output_table tbl; /* Output table layout data */
};

struct reader {
	FILE *fp; /* File with profiler stream */
	size_t pos; /* Current position in buffer */
	char *buf; /* Buffer with variable size */
	int eof; /* EOF reached flag */
};

struct vmstate_stats {
	uint64_t count;
	uint64_t total_state;
	uint64_t total_all;
};

struct parser_state {
	struct reader reader /* Low-level reader */;
	char *profile_name; /* What to profile */
	char *cg_file_name; /* Callgraph output path */
	char *exec_file_name; /* Re-defined executable path */
	uint64_t start_time; /* Time when profiling started */
	uint64_t internal_id; /* ID for cachegrind callgraph */
	size_t _vmstates[UJ_PROFILE_NUM_DISTINCT_VM_STATES];
	size_t vmstates_sorted[UJ_PROFILE_NUM_DISTINCT_VM_STATES];
	size_t vmstates_demangle[UJ_PROFILE_NUM_DISTINCT_VM_STATES];
	struct output_table table_lfunc; /* Human-readable tables params */
	struct output_table table_cfunc;
	struct output_table table_ffunc;
	struct output_table table_trace;
	struct output_table table_vmstate;
	struct output_table table_so;
	struct vector vec_cfunc; /* Holds info about top functions */
	struct vector vec_lfunc;
	struct vector vec_ffunc;
	struct vector vec_trace;
	struct hvmstate_info hvmst_infos[UJ_PROFILE_NUM_DISTINCT_VM_STATES];
	struct vector vec_lfunc_cache; /* Holds info about all cached funcs*/
	struct vector vec_cfunc_cache;
	struct vector vec_ffunc_cache;
	struct vector vec_loaded_so;
	struct hash ht_trace;
	struct hash ht;
	uint64_t total; /* Total amount of profiled states */
};

#endif /* !_UJPP_MAIN_H */
