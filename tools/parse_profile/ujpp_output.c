/*
 * This module implements functions for printing results after parsing.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "ujpp_main.h"
#include "ujpp_utils.h"
#include "ujpp_demangle_lua.h"
#include "ujpp_output.h"
#include "../../src/uj_vmstate.h"

#define BUFF_SIZE 4096

static void output_print_so_list_item(const struct parser_state *ps,
				      const struct shared_obj *so)
{
	char buf[BUFF_SIZE];

	snprintf(buf, sizeof(buf), "| %s", so->path);
	ujpp_table_row(&ps->table_so, 0, buf);

	sprintf(buf, " 0x%" PRIx64, so->base);
	ujpp_table_row(&ps->table_so, 1, buf);

	if (!so->found)
		sprintf(buf, " no");
	else
		sprintf(buf, " yes");
	ujpp_table_row(&ps->table_so, 2, buf);
	ujpp_table_row(&ps->table_so, 3, "");
	printf("\n");
}

static void output_print_so_list(struct parser_state *ps)
{
	printf("\nLoaded shared objects:\n");

	ujpp_table_so_length(ps);
	ujpp_table_header(&ps->table_so);

	for (size_t i = 0; i < ps->vec_loaded_so.size; i++) {
		const struct shared_obj *so = ps->vec_loaded_so.elems[i];

		output_print_so_list_item(ps, so);
	}

	ujpp_table_bottom(&ps->table_so);
	printf("\n");
}

static void output_print_columns(const struct output_table *tbl,
				 const char *buf1, const char *buf2,
				 const char *buf3, const char *buf4)
{
	ujpp_table_row(tbl, 0, buf1);
	ujpp_table_row(tbl, 1, buf2);
	ujpp_table_row(tbl, 2, buf3);
	ujpp_table_row(tbl, 3, buf4);
	printf("\n");
}

static void output_bottom(struct output_table *tbl,
			  const struct vmstate_stats *stats)
{
	char buf[BUFF_SIZE];

	ujpp_table_row(tbl, 0, "| TOTAL");
	sprintf(buf, " %" PRIu64, stats->total_state);
	ujpp_table_row(tbl, 1, buf);
	sprintf(buf, " %0.4f", 100.0);
	ujpp_table_row(tbl, 2, buf);
	sprintf(buf, " %0.4f", 100.0 * stats->total_state / stats->total_all);
	ujpp_table_row(tbl, 3, buf);
	printf("\n");
	ujpp_table_bottom(tbl);
	printf("\n");
}

static void output_prologue(struct output_table *t, const char *name)
{
	printf("%s\n", name);
	ujpp_table_header(t);
}

static void output_middle(struct output_table *t, const char *column0,
			  const struct vmstate_stats *stats)
{
	char column2[BUFF_SIZE];
	char column3[BUFF_SIZE];
	char column4[BUFF_SIZE];

	sprintf(column2, " %lu", stats->count);
	sprintf(column3, " %0.4f", 100.0 * stats->count / stats->total_state);
	sprintf(column4, " %0.4f", 100.0 * stats->count / stats->total_all);
	output_print_columns(t, column0, column2, column3, column4);
}

static void output_print_vmstates(struct parser_state *ps)
{
	struct vmstate_stats stats = {
		.total_state = ps->total,
		.total_all = ps->total,
	};

	ujpp_table_vmstates_length(ps, stats.total_all);
	output_prologue(&ps->table_vmstate, "vmstate counters:");

	for (size_t i = 0; i < UJ_PROFILE_NUM_DISTINCT_VM_STATES; i++) {
		stats.count = ps->vmstates_sorted[i];
		const char *vmst_name =
			ujpp_utils_vmst_counter_name(ps, stats.count);
		char column1[BUFF_SIZE];

		sprintf(column1, "| %s", vmst_name);
		output_middle(&ps->table_vmstate, column1, &stats);
	}

	output_bottom(&ps->table_vmstate, &stats);
}

static void output_print_lfunc(struct parser_state *ps)
{
	struct vmstate_stats stats = {
		.total_state = ps->_vmstates[UJ_VMST_LFUNC],
		.total_all = ps->total,
	};

	ujpp_table_lfunc_length(ps);
	/* must be after *_length call */
	output_prologue(&ps->table_lfunc, "lfunc counters:");

	for (size_t i = 0; i < ujpp_vector_size(&ps->vec_lfunc); i++) {
		const struct lfunc *lf = ujpp_vector_at(&ps->vec_lfunc, i);
		const struct lfunc_cache *lf_c =
			ujpp_vector_at(&ps->vec_lfunc_cache, lf->cache_id);
		char column1[BUFF_SIZE];

		assert(NULL != lf_c->demangled_sym);

		if (DEMANGLE_LUA_FAILED != lf_c->demangled_sym) {
			sprintf(column1, "| %s (%s:%" PRIu64 ")",
				lf_c->demangled_sym, lf_c->sym, lf_c->line);
		} else {
			sprintf(column1, "| %s:%" PRIu64 "", lf_c->sym,
				lf_c->line);
		}
		stats.count = lf->count;
		output_middle(&ps->table_lfunc, column1, &stats);
	}

	output_bottom(&ps->table_lfunc, &stats);
}

static void output_print_cfunc(struct parser_state *ps)
{
	ujpp_table_cfunc_length(ps);
	struct vmstate_stats stats = {
		.total_state = ps->_vmstates[UJ_VMST_CFUNC],
		.total_all = ps->total,
	};
	output_prologue(&ps->table_cfunc, "cfunc counters:");

	for (size_t i = 0; i < ujpp_vector_size(&ps->vec_cfunc); i++) {
		const struct cfunc *cf = ujpp_vector_at(&ps->vec_cfunc, i);
		const struct cfunc_cache *cache =
			ujpp_vector_at(&ps->vec_cfunc_cache, cf->cache_id);
		char column1[BUFF_SIZE];

		if (cache->symbol)
			snprintf(column1, sizeof(column1), "| %s",
				 cache->symbol);
		else
			snprintf(column1, sizeof(column1), "| 0x%" PRIx64,
				 cache->addr);
		stats.count = cf->count;
		output_middle(&ps->table_cfunc, column1, &stats);
	}

	output_bottom(&ps->table_cfunc, &stats);
}

static void output_print_ffunc(struct parser_state *ps)
{
	ujpp_table_ffunc_length(ps);
	struct vmstate_stats stats = {
		.total_state = ps->_vmstates[UJ_VMST_FFUNC],
		.total_all = ps->total,
	};
	output_prologue(&ps->table_ffunc, "ffunc counters:");

	for (size_t i = 0; i < ujpp_vector_size(&ps->vec_ffunc); i++) {
		const struct ffunc *ff = ujpp_vector_at(&ps->vec_ffunc, i);
		const struct ffunc_cache *ff_c =
			ujpp_vector_at(&ps->vec_ffunc_cache, ff->cache_id);
		char column1[BUFF_SIZE];

		sprintf(column1, "| %s", ujpp_utils_ffunc_name(ff_c->ffid));
		stats.count = ff->count;
		output_middle(&ps->table_ffunc, column1, &stats);
	}

	output_bottom(&ps->table_ffunc, &stats);
}

static void output_print_trace(struct parser_state *ps)
{
	struct vmstate_stats stats = {
		.total_state = ps->_vmstates[UJ_VMST_TRACE],
		.total_all = ps->total,
	};

	ujpp_table_trace_length(ps);
	output_prologue(&ps->table_trace, "trace counters:");

	for (size_t i = 0; i < ujpp_vector_size(&ps->vec_trace); i++) {
		const struct trace *t = ujpp_vector_at(&ps->vec_trace, i);
		char column1[BUFF_SIZE];

		sprintf(column1, "| %" PRIu64 "-%" PRIu64 " %s:%" PRIu64,
			t->generation, t->traceno, t->name, t->line);
		stats.count = t->count;
		output_middle(&ps->table_trace, column1, &stats);
	}

	output_bottom(&ps->table_trace, &stats);
}

static const char *const vmstate_headers[] = {
	"lfunc counters:", /* UJ_VMST_LFUNC */
	"ffunc counters:", /* UJ_VMST_FFUNC */
	"cfunc counters:", /* UJ_VMST_CFUNC */
	"idle counters:", /* UJ_VMST_IDLE */
	"interp counters:", /* UJ_VMST_INTERP */
	"gc counters:", /* UJ_VMST_GC */
	"exit counters:", /* UJ_VMST_EXIT */
	"record counters:", /* UJ_VMST_RECORD */
	"opt counters:", /* UJ_VMST_OPT */
	"asm counters:", /* UJ_VMST_ASM */
	"trace counters:" /* UJ_VMST_TRACE */
};

static void output_print_hvmstate(struct parser_state *ps, enum vmstate vmstate)
{
	struct hvmstate_info *hvmsti = &ps->hvmst_infos[vmstate];
	struct vmstate_stats stats = {
		.total_state = ps->_vmstates[vmstate],
		.total_all = ps->total,
	};

	ujpp_table_hvmstate_length(ps, vmstate);
	output_prologue(&hvmsti->tbl, vmstate_headers[vmstate]);

	for (size_t i = 0; i < ujpp_vector_size(&hvmsti->vec_rip); i++) {
		const struct hvmstate *hvmst =
			ujpp_vector_at(&hvmsti->vec_rip, i);
		char column1[BUFF_SIZE];

		if (hvmst->di.so_idx != -1) {
			struct shared_obj *so = ujpp_vector_at(
				&ps->vec_loaded_so, hvmst->di.so_idx);
			snprintf(column1, sizeof(column1), "| %s @",
				 so->short_name);
		} else {
			snprintf(column1, sizeof(column1),
				 NATIVE_SYM_NOT_FOUND);
		}

		if (ujpp_demangle_valid(hvmst)) {
			snprintf(&column1[strlen(column1)],
				 sizeof(column1) - strlen(column1) - 1, " %s",
				 hvmst->symbol);
		} else {
			snprintf(&column1[strlen(column1)],
				 sizeof(column1) - strlen(column1) - 1,
				 " 0x%" PRIx64, hvmst->addr);
		}
		stats.count = hvmst->count;

		output_middle(&hvmsti->tbl, column1, &stats);
	}

	output_bottom(&hvmsti->tbl, &stats);
}

static void output_print_hvmstates(struct parser_state *ps)
{
	for (size_t hvmst = UJ_VMST_HVMST_START; hvmst < UJ_VMST__MAX; hvmst++)
		output_print_hvmstate(ps, hvmst);
}

void ujpp_output_print(struct parser_state *ps)
{
	ujpp_utils_sort_items(ps);
	output_print_so_list(ps);
	output_print_vmstates(ps);
	output_print_lfunc(ps);
	output_print_cfunc(ps);
	output_print_ffunc(ps);
	output_print_trace(ps);
	output_print_hvmstates(ps);
}
