/*
 * Interfaces for printing results after parsing.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJPP_OUTPUT_H
#define _UJPP_OUTPUT_H

struct parser_state;

/* Prints info about vmstates, lfunc, cfuncs, etc... */
void ujpp_output_print(struct parser_state *ps);

#define NATIVE_SYM_NOT_FOUND "| not_found @"
#define NATIVE_SYM_NOT_FOUND_LEN strlen(NATIVE_SYM_NOT_FOUND)

#endif /* !_UJPP_OUTPUT_H */
