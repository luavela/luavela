/*
 * Various utility functions for C-level dumpers.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "uj_proto.h"

#include "utils/lj_char.h"
#include "utils/str.h"

#include "dump/uj_dump_datadef.h"

#define FF_NAME_BUFFER_SIZE 80

static void dump_prepare_ff_name(const GCfunc *fn, char *ff_name)
{
	to_lower(ff_name, dump_ff_names[(fn)->c.ffid]);
	replace_underscores(ff_name);
}

/* Dump pretty-formatted fast function name to out. */
static void dump_ff_name(FILE *out, const GCfunc *fn)
{
	char ff_name[FF_NAME_BUFFER_SIZE];

	dump_prepare_ff_name(fn, ff_name);
	fprintf(out, "%s", ff_name);
}

/* Print pretty-formatted fast function name to buf. */
static size_t dump_format_ff_name(char *buf, const GCfunc *fn)
{
	char ff_name[FF_NAME_BUFFER_SIZE];

	dump_prepare_ff_name(fn, ff_name);
	return (size_t)sprintf(buf, "%s", ff_name);
}

void uj_dump_nonluafunc_description(FILE *out, const GCfunc *fn)
{
	lua_assert(!isluafunc(fn));

	UJ_PEDANTIC_OFF
	/* casting a function ptr to void* */
	if (iscfunc(fn))
		fprintf(out, "C:%p", (void *)fn->c.f);
	else if (isffunc(fn))
		dump_ff_name(out, fn);
	else
		fprintf(out, "(?)");
	UJ_PEDANTIC_ON
}

void uj_dump_func_description(FILE *out, const GCfunc *fn, BCPos pc)
{
	if (isluafunc(fn))
		uj_proto_fprintloc(out, funcproto(fn), pc);
	else
		uj_dump_nonluafunc_description(out, fn);
}

void uj_dump_format_func_description(char *buf, const GCfunc *fn, BCPos pc)
{
	UJ_PEDANTIC_OFF
	/* casting a function ptr to void* */
	if (isluafunc(fn))
		uj_proto_sprintloc(buf, funcproto(fn), pc);
	else if (iscfunc(fn))
		sprintf(buf, "C:%p", (void *)fn->c.f);
	else if (isffunc(fn))
		dump_format_ff_name(buf, fn);
	else
		sprintf(buf, "(?)");
	UJ_PEDANTIC_ON
}

size_t uj_dump_format_kstr(char *fmt_buf, const char *val,
			   size_t copy_threshold)
{
	size_t i;
	int strip = 0;
	const size_t val_len = strlen(val);
	const char *fmt_start;

	*(fmt_buf++) = '"';
	fmt_start = fmt_buf;

	for (i = 0; i < val_len; i++) {
		const unsigned char c = val[i];

		if ((size_t)(fmt_buf - fmt_start) >= copy_threshold) {
			strip = 1;
			break;
		}

		if (LJ_LIKELY(!lj_char_iscntrl(c))) {
			*(fmt_buf++) = c;
			continue;
		}

		*(fmt_buf)++ = '\\';
		switch (c) {
		case '\n': {
			*(fmt_buf)++ = 'n';
			break;
		}
		case '\r': {
			*(fmt_buf)++ = 'r';
			break;
		}
		case '\t': {
			*(fmt_buf)++ = 't';
			break;
		}
		default: {
			sprintf(fmt_buf, "%03d", c);
			fmt_buf += 3;
		}
		}
	}

	*(fmt_buf++) = '"';
	if (strip)
		*(fmt_buf++) = '~';

	return (size_t)(fmt_buf - fmt_start) + 1; /* +1 for initial " char */
}
