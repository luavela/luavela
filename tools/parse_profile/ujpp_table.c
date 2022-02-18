/*
 * Human-readable table.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <string.h>

#include "ujpp_main.h"
#include "ujpp_utils.h"
#include "ujpp_demangle_lua.h"
#include "ujpp_output.h"
#include "../../src/uj_vmstate.h"

#define DEFAULT_ROW_LEN 12
#define SPACE_NUM 3 /* '|' and 2 spaces. */
#define MAX_HEX_ADDR_CHARS_NUM 18 /* Hex value + 2 symbols for '0x'. */
#define NOT_RESOLVED_SYM_LEN (MAX_HEX_ADDR_CHARS_NUM + SPACE_NUM)

static size_t table_digits(size_t n)
{
	size_t count = 0;

	while (n) {
		n /= 10;
		count++;
	}
	return count;
}

static void table_symbol(size_t n, const char *s)
{
	while (n-- > 0)
		printf("%s", s);
}

static void table_format(const char *str, const size_t max_len)
{
	size_t str_len = strlen(str);
	const size_t delta = max_len - str_len;

	if (max_len < str_len)
		ujpp_utils_die("wrong column length: max_len %zu, str_len %zu",
			       max_len, str_len);

	printf("%s", str);
	table_symbol(delta, " ");
	printf("|");
}

void ujpp_table_header(const struct output_table *tbl)
{
	table_symbol(1, "+");
	table_symbol(tbl->row_length[0] - 1, "-");
	table_symbol(1, "+");
	table_symbol(tbl->row_length[1], "-");
	table_symbol(1, "+");
	table_symbol(12, "-");
	table_symbol(1, "+");
	table_symbol(12, "-");
	table_symbol(1, "+");
	printf("\n");
	table_format(tbl->names[0], tbl->row_length[0]);
	table_format(tbl->names[1], tbl->row_length[1]);
	table_format(tbl->names[2], 12);
	table_format(tbl->names[3], 12);
	printf("\n");
	table_symbol(1, "+");
	table_symbol(tbl->row_length[0] - 1, "-");
	table_symbol(1, "+");
	table_symbol(tbl->row_length[1], "-");
	table_symbol(1, "+");
	table_symbol(12, "-");
	table_symbol(1, "+");
	table_symbol(12, "-");
	table_symbol(1, "+");
	printf("\n");
}

void ujpp_table_bottom(const struct output_table *tbl)
{
	table_symbol(1, "+");
	table_symbol(tbl->row_length[0] - 1, "-");
	table_symbol(1, "+");
	table_symbol(tbl->row_length[1], "-");
	table_symbol(1, "+");
	table_symbol(12, "-");
	table_symbol(1, "+");
	table_symbol(12, "-");
	table_symbol(1, "+");
	printf("\n");
}

void ujpp_table_row(const struct output_table *tbl, const uint8_t column,
		    const char *str)
{
	table_format(str, tbl->row_length[column]);
}

static void table_fix_lenghts(struct output_table *tbl)
{
	if (tbl->row_length[0] < DEFAULT_ROW_LEN)
		tbl->row_length[0] = DEFAULT_ROW_LEN;

	if (tbl->row_length[1] < DEFAULT_ROW_LEN)
		tbl->row_length[1] = DEFAULT_ROW_LEN;
}

size_t table_calc_cfunc_len(struct parser_state *ps, size_t i)
{
	const struct cfunc *cf = ujpp_vector_at(&ps->vec_cfunc, i);
	const struct cfunc_cache *cache =
		ujpp_vector_at(&ps->vec_cfunc_cache, cf->cache_id);

	return cache->symbol ? strlen(cache->symbol) + SPACE_NUM :
			       NOT_RESOLVED_SYM_LEN;
}

void ujpp_table_cfunc_length(struct parser_state *ps)
{
	/* Number of symbols in first column. */
	size_t column1_len = DEFAULT_ROW_LEN;

	for (size_t i = 0; i < ujpp_vector_size(&ps->vec_cfunc); i++) {
		size_t len = table_calc_cfunc_len(ps, i);

		if (len > column1_len)
			column1_len = len;
	}

	ps->table_cfunc.row_length[0] = column1_len;
	ps->table_cfunc.row_length[1] =
		table_digits(ps->_vmstates[UJ_VMST_CFUNC]) + SPACE_NUM;
	ps->table_cfunc.row_length[2] = DEFAULT_ROW_LEN;
	ps->table_cfunc.row_length[3] = DEFAULT_ROW_LEN;
	table_fix_lenghts(&ps->table_cfunc);
}

void ujpp_table_trace_length(struct parser_state *ps)
{
	ps->table_trace.row_length[0] = DEFAULT_ROW_LEN;
	for (size_t i = 0; i < ps->vec_trace.size; i++) {
		const struct trace *t = ps->vec_trace.elems[i];
		const size_t len = strlen(t->name) + table_digits(t->line) +
				   table_digits(t->traceno) +
				   table_digits(t->generation) + 2 * SPACE_NUM;

		if (len > ps->table_trace.row_length[0])
			ps->table_trace.row_length[0] = len;
	}

	ps->table_trace.row_length[1] =
		table_digits(ps->_vmstates[UJ_VMST_TRACE]) + SPACE_NUM;
	ps->table_trace.row_length[2] = DEFAULT_ROW_LEN;
	ps->table_trace.row_length[3] = DEFAULT_ROW_LEN;
	table_fix_lenghts(&ps->table_trace);
}

void ujpp_table_ffunc_length(struct parser_state *ps)
{
	ps->table_ffunc.row_length[0] = DEFAULT_ROW_LEN;
	for (size_t i = 0; i < ps->vec_ffunc.size; i++) {
		const struct ffunc *ff = ps->vec_ffunc.elems[i];
		const struct ffunc_cache *ff_c =
			ps->vec_ffunc_cache.elems[ff->cache_id];
		const size_t len =
			strlen(ujpp_utils_ffunc_name(ff_c->ffid)) + SPACE_NUM;

		if (len > ps->table_ffunc.row_length[0])
			ps->table_ffunc.row_length[0] = len;
	}

	ps->table_ffunc.row_length[1] =
		table_digits(ps->_vmstates[UJ_VMST_FFUNC]) + SPACE_NUM;
	ps->table_ffunc.row_length[2] = DEFAULT_ROW_LEN;
	ps->table_ffunc.row_length[3] = DEFAULT_ROW_LEN;
	table_fix_lenghts(&ps->table_ffunc);
}

void ujpp_table_hvmstate_length(struct parser_state *ps, enum vmstate vmstate)
{
	struct hvmstate_info *hvmsti = &ps->hvmst_infos[vmstate];

	hvmsti->tbl.row_length[0] = DEFAULT_ROW_LEN;
	for (size_t i = 0; i < hvmsti->vec_rip.size; i++) {
		const struct hvmstate *hvmst = hvmsti->vec_rip.elems[i];
		const struct shared_obj *so;
		size_t len = 0;

		if (hvmst->di.so_idx != -1) {
			so = ps->vec_loaded_so.elems[hvmst->di.so_idx];
			/* 4 is a '|', '@' and 2 spaces. */
			len += strlen(so->short_name) + 4;
		} else {
			len += NATIVE_SYM_NOT_FOUND_LEN;
		}

		if (ujpp_demangle_valid(hvmst))
			len += strlen(hvmst->symbol);
		else
			len += MAX_HEX_ADDR_CHARS_NUM;

		len += SPACE_NUM;

		if (len > hvmsti->tbl.row_length[0])
			hvmsti->tbl.row_length[0] = len;
	}

	hvmsti->tbl.row_length[1] =
		table_digits(ps->_vmstates[vmstate]) + SPACE_NUM;
	hvmsti->tbl.row_length[2] = DEFAULT_ROW_LEN;
	hvmsti->tbl.row_length[3] = DEFAULT_ROW_LEN;
	table_fix_lenghts(&hvmsti->tbl);
}

void ujpp_table_vmstates_length(struct parser_state *ps, size_t total)
{
	ps->table_vmstate.row_length[0] = DEFAULT_ROW_LEN;
	ps->table_vmstate.row_length[1] = table_digits(total) + SPACE_NUM;
	ps->table_vmstate.row_length[2] = DEFAULT_ROW_LEN;
	ps->table_vmstate.row_length[3] = DEFAULT_ROW_LEN;
	table_fix_lenghts(&ps->table_vmstate);
}

void ujpp_table_lfunc_length(struct parser_state *ps)
{
	size_t len = DEFAULT_ROW_LEN;

	for (size_t i = 0; i < ps->vec_lfunc.size; i++) {
		const struct lfunc *lf = ps->vec_lfunc.elems[i];
		const struct lfunc_cache *lf_c =
			ps->vec_lfunc_cache.elems[lf->cache_id];
		size_t demangled_len = DEMANGLE_LUA_FAILED ==
						       lf_c->demangled_sym ?
					       0 :
					       strlen(lf_c->demangled_sym);

		len = demangled_len + strlen(lf_c->sym) +
		      table_digits(lf_c->line) + 8;

		if (len > ps->table_lfunc.row_length[0])
			ps->table_lfunc.row_length[0] = len;
	}

	ps->table_lfunc.row_length[1] =
		table_digits(ps->_vmstates[UJ_VMST_LFUNC]) + SPACE_NUM;
	ps->table_lfunc.row_length[2] = DEFAULT_ROW_LEN;
	ps->table_lfunc.row_length[3] = DEFAULT_ROW_LEN;
	table_fix_lenghts(&ps->table_lfunc);
}

void ujpp_table_so_length(struct parser_state *ps)
{
	ps->table_so.row_length[0] = DEFAULT_ROW_LEN;
	/* 2 spaces for indentation. */
	ps->table_so.row_length[1] = MAX_HEX_ADDR_CHARS_NUM + 2;
	ps->table_so.row_length[2] = DEFAULT_ROW_LEN;
	ps->table_so.row_length[3] = DEFAULT_ROW_LEN;

	for (size_t i = 0; i < ps->vec_loaded_so.size; i++) {
		const struct shared_obj *so = ps->vec_loaded_so.elems[i];
		const size_t len = strlen(so->path) + SPACE_NUM;

		if (len > ps->table_so.row_length[0])
			ps->table_so.row_length[0] = len;
	}

	table_fix_lenghts(&ps->table_so);
}
