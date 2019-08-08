/*
 * Interface for cachegrind call-graph creation.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJPP_CALLGRAPH_H
#define _UJPP_CALLGRAPH_H

struct parser_state;

/* Generates call-graph in cachegrind format */
void ujpp_callgraph_generate(struct parser_state *ps);

#endif /* !_UJPP_CALLGRAPH_H */
