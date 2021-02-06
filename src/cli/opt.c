/*
 * Simplistic parsing of CLI arguments.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <string.h>

#include "cli/opt.h"

#include "luaconf.h"
#include "lextlib.h"

#define EXT_OPTION_SWITCH "-X"

#define ERR_EMPTY_OPTION "No option specified for %s"
#define ERR_UNKNOWN_VALUE "Unknown value: %s"
#define ERR_UNKNOWN_OPTION "Unknown option: %s"

#define HASHF_PREFIX "hashf="
#define HASHF_MURMUR HASHF_PREFIX "murmur"
#define HASHF_CITY HASHF_PREFIX "city"

#define ITERN_PREFIX "itern="
#define ITERN_ON ITERN_PREFIX "on"
#define ITERN_OFF ITERN_PREFIX "off"

static int opt_is_prefixed(const char *s, const char *prefix)
{
	lua_assert(s != NULL);
	lua_assert(prefix != NULL);
	return strncmp(prefix, s, strlen(prefix)) == 0;
}

static enum opt_parse_status opt_set_hashf(const char *kv,
					   struct luae_Options *opt)
{
	if (strcmp(kv, HASHF_MURMUR) == 0) {
		opt->hashftype = LUAE_HASHF_MURMUR;
		return OPT_PARSE_OK;
	} else if (strcmp(kv, HASHF_CITY) == 0) {
		opt->hashftype = LUAE_HASHF_CITY;
		return OPT_PARSE_OK;
	}

	return OPT_PARSE_ERROR;
}

static enum opt_parse_status opt_set_itern(const char *kv,
					   struct luae_Options *opt)
{
	if (strcmp(kv, ITERN_ON) == 0) {
		opt->disableitern = 0;
		return OPT_PARSE_OK;
	} else if (strcmp(kv, ITERN_OFF) == 0) {
		opt->disableitern = 1;
		return OPT_PARSE_OK;
	}

	return OPT_PARSE_ERROR;
}

typedef enum opt_parse_status (*opt_setter_func)(const char *,
						 struct luae_Options *);

struct opt_setter_map {
	const char *prefix;
	opt_setter_func opt_setter;
};

static const struct opt_setter_map opt_setters[] = {
	{HASHF_PREFIX, opt_set_hashf},
	{ITERN_PREFIX, opt_set_itern}};

enum opt_parse_status cli_opt_parse_kv(const char *kv, struct luae_Options *opt,
				       char *buffer, size_t n)
{
	opt_setter_func opt_setter;
	size_t i;

	if (kv == NULL || strlen(kv) == 0) {
		snprintf(buffer, n, ERR_EMPTY_OPTION, EXT_OPTION_SWITCH);
		return OPT_PARSE_ERROR;
	}

	if (opt_is_prefixed(kv, EXT_OPTION_SWITCH))
		kv += strlen(EXT_OPTION_SWITCH);

	/* Dispatch to a concrete option setter from the static map: */
	opt_setter = NULL;
	for (i = 0; i < sizeof(opt_setters) / sizeof(opt_setters[0]); i++) {
		if (opt_is_prefixed(kv, opt_setters[i].prefix)) {
			opt_setter = opt_setters[i].opt_setter;
			break;
		}
	}

	if (opt_setter == NULL) {
		snprintf(buffer, n, ERR_UNKNOWN_OPTION, kv);
		return OPT_PARSE_ERROR;
	}

	if (opt_setter(kv, opt) == OPT_PARSE_OK)
		return OPT_PARSE_OK;

	snprintf(buffer, n, ERR_UNKNOWN_VALUE, kv);
	return OPT_PARSE_ERROR;
}
