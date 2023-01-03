/*
 * Interfaces for printing human-readable tables.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJPP_TABLE_H
#define _UJPP_TABLE_H

#include <stdio.h>
#include <stdint.h>

#include "uj_vmstate.h"

#define NUM_COLUMNS 4

struct output_table {
	size_t row_length[NUM_COLUMNS];
	char *names[NUM_COLUMNS];
};

struct parser_state;

/* Functions for primting human-readable tables */
void ujpp_table_header(const struct output_table *tbl);
void ujpp_table_bottom(const struct output_table *tbl);
void ujpp_table_row(const struct output_table *ot, const uint8_t column,
		    const char *str);
/* This functions is used to calculate length for each column */
void ujpp_table_cfunc_length(struct parser_state *ps);
void ujpp_table_hvmstate_length(struct parser_state *ps, enum vmstate vmstate);
void ujpp_table_ffunc_length(struct parser_state *ps);
void ujpp_table_trace_length(struct parser_state *ps);
void ujpp_table_vmstates_length(struct parser_state *ps, size_t total);
void ujpp_table_lfunc_length(struct parser_state *ps);
void ujpp_table_so_length(struct parser_state *ps);

#endif /* !_UJPP_TABLE_H */
