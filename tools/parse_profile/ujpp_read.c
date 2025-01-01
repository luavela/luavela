/*
 * This module provides low-level stream reading functionality.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "ujpp_read.h"
#include "ujpp_utils.h"
#include "ujpp_main.h"
#include "../../src/utils/leb128.h"

/* 5 MB buffer should be enough. */
#define INPUT_BUFFER_SIZE (5 * 1024 * 1024)

void ujpp_read_init(struct reader *r, const char *fname)
{
	if (NULL == fname)
		ujpp_utils_die("wrong profile name", NULL);

	memset(r, 0, sizeof(*r));

	r->fp = fopen(fname, "rb");

	if (NULL == r->fp)
		ujpp_utils_die("can't open file: %s\n", fname);

	r->buf = ujpp_utils_allocz(INPUT_BUFFER_SIZE);

	if (fread(r->buf, 1, INPUT_BUFFER_SIZE, r->fp) != INPUT_BUFFER_SIZE)
		r->eof = 1;
}

void ujpp_read_terminate(struct reader *r)
{
	fclose(r->fp);
	free(r->buf);
}

static int read_buffer_has(const struct reader *r, size_t n)
{
	return INPUT_BUFFER_SIZE - r->pos - 1 >= n;
}

static void read_buffer(struct reader *r)
{
	size_t delta = INPUT_BUFFER_SIZE - r->pos;

	memcpy(r->buf, r->buf + r->pos, delta);
	r->pos = 0;

	if (r->eof)
		ujpp_utils_die("EOF reached", NULL);

	if (fread(r->buf + delta, 1, INPUT_BUFFER_SIZE - delta, r->fp) !=
	    INPUT_BUFFER_SIZE - delta)
		r->eof = 1;
}

uint64_t ujpp_read_u64(struct reader *r)
{
	size_t ret;
	uint64_t out;

	if (!read_buffer_has(r, sizeof(uint64_t)))
		/* Number splited, need to read buffer */
		read_buffer(r);

	assert(read_buffer_has(r, sizeof(uint64_t)));

	ret = read_uleb128(&out, (uint8_t *)r->buf + r->pos);
	r->pos += ret;

	return out;
}

uint8_t ujpp_read_u8(struct reader *r)
{
	if (!read_buffer_has(r, sizeof(uint8_t)))
		read_buffer(r);

	assert(read_buffer_has(r, sizeof(uint8_t)));
	return r->buf[r->pos++];
}

char *ujpp_read_str(struct reader *r)
{
	uint64_t len = ujpp_read_u64(r);
	char *str = ujpp_utils_allocz((len + 1));

	if (!read_buffer_has(r, len))
		read_buffer(r);

	assert(read_buffer_has(r, len));

	memcpy(str, r->buf + r->pos, len);
	r->pos += len;
	str[len] = '\0';
	return str;
}
