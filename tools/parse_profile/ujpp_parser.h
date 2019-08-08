/*
 * This module is used to read streamed samples.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJPP_PARSER_H
#define _UJPP_PARSER_H

#include <inttypes.h>

struct parser_state;

void ujpp_parser_read_stack(struct parser_state *ps, uint8_t vmstate);

extern const char *LUA_MAIN;

#endif /* !_UJPP_PARSER_H */
