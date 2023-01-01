/*
 * Interfaces for initializing parser_state and memory management/
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJPP_STATE_H
#define _UJPP_STATE_H

struct parser_state;

/* Free all allocated memory */
void ujpp_state_free(struct parser_state *ps);
/* Basic initializations - allocating memory for vectors, hash tables, etc */
void ujpp_state_init(struct parser_state *p, int argc, char **argv,
		     const char *short_opt, const struct option *long_opt);

#endif /* !_UJPP_STATE_H */
