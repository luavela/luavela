/*
 * Parsing of CLI arguments.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJIT_CLI_OPT_H
#define _UJIT_CLI_OPT_H

#include <stddef.h>

struct luae_Options;

enum opt_parse_status { OPT_PARSE_OK, OPT_PARSE_ERROR };

/*
 * Parses a single key-value argument. In case of success, sets a corresponding
 * value in the output opt structure and returns CLI_OPT_PARSE_OK. Otherwise
 * returns CLI_OPT_PARSE_ERROR and prints the error message to the buffer that
 * must be capable to store at least n bytes.
 */
enum opt_parse_status cli_opt_parse_kv(const char *kv, struct luae_Options *opt,
				       char *buffer, size_t n);

#endif /* !_UJIT_CLI_OPT_H */
