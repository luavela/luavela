/*
 * Low-level writer for uJIT Profiler.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <unistd.h>

#include "profile/ujp_write.h"
#include "utils/leb128.h"
#include "lj_def.h"

/*
 * memcpy from the standard C library is not guaranteed to be async-signal-safe.
 * Common sense might tell us that it should be async-signal-safe (at least
 * because it is unlikely that memcpy will allocate anything dynamically),
 * but the standard is always the standard. So uJIT offers its own
 * implementation of memcpy. Of course it will never be as fast as the system
 * memcpy, but it will be guaranteed to always stay async-signal-safe. Feel
 * free to remove the define below to fall-back to the system memcpy if the
 * custom implementation becomes a botlleneck, but this is at your own risk.
 * You are warned:)
 */
#define USE_CUSTOM_UJIT_MEMCPY

#ifdef USE_CUSTOM_UJIT_MEMCPY
/*
 * Behaves exactly as memcpy from the standard C library with following caveats:
 * - Guaranteed to be async-signal-safe.
 * - Does *not* handle unaligned byte stores.
 * - src cannot be declared as (const void *restrict) unless we start
 *   supporting C99 explicitly. Possible overlapping of dst in src is ignored.
 */
#define COPY_BUFFER_TYPE uint64_t
#define COPY_BUFFER_SIZE sizeof(COPY_BUFFER_TYPE)

static void *write_memcpy(void *dst, const void *src, size_t n)
{
	size_t loops;
	size_t i;
	uint8_t *dst_pos = (uint8_t *)dst;
	const uint8_t *src_pos = (const uint8_t *)src;

	loops = n / COPY_BUFFER_SIZE;
	for (i = 0; i < loops; i++) {
		*(COPY_BUFFER_TYPE *)dst_pos =
			*(const COPY_BUFFER_TYPE *)src_pos;
		dst_pos += COPY_BUFFER_SIZE;
		src_pos += COPY_BUFFER_SIZE;
	}

	loops = n % COPY_BUFFER_SIZE;
	for (i = 0; i < loops; i++) {
		*dst_pos = *src_pos;
		dst_pos++;
		src_pos++;
	}

	return dst;
}
#else /* !USE_CUSTOM_UJIT_MEMCPY */
#define write_memcpy memcpy
#endif /* USE_CUSTOM_UJIT_MEMCPY */

static LJ_AINLINE void write_set_io_error(struct ujp_buffer *buf)
{
	buf->flags |= STREAM_ERR_IO;
}

static LJ_AINLINE void write_set_frame_type(uint8_t *frame_type, uint8_t type)
{
	*frame_type |= (type << 3);
}

static LJ_AINLINE void write_set_frame_flag(uint8_t *frame_type, uint8_t flag)
{
	*frame_type |= flag;
}

/* Wraps a write syscall ensuring all data have been written. */
static void write_buffer_sys(struct ujp_buffer *buffer, const void *buf,
			     size_t buf_len)
{
	int fd = buffer->fd;
	ssize_t written;

	for (;;) {
		written = write(fd, buf, buf_len);

		if (LJ_UNLIKELY(written == -1)) {
			write_set_io_error(buffer);
			return;
		}

		if ((size_t)written == buf_len)
			return;

		buf = (uint8_t *)buf + (ptrdiff_t)written;
		buf_len -= (size_t)written;
	}
}

static LJ_AINLINE size_t write_bytes_buffered(const struct ujp_buffer *buf)
{
	return buf->pos - buf->buf;
}

static LJ_AINLINE int write_buffer_has(const struct ujp_buffer *buf, size_t n)
{
	return (buf->size - write_bytes_buffered(buf)) >= n;
}

void ujp_write_flush_buffer(struct ujp_buffer *buf)
{
	write_buffer_sys(buf, buf->buf, write_bytes_buffered(buf));
	buf->pos = buf->buf;
}

void ujp_write_init(struct ujp_buffer *buf, int fd, uint8_t *mem, size_t size)
{
	buf->fd = fd;
	buf->buf = mem;
	buf->pos = mem;
	buf->size = size;
	buf->flags = 0;
}

void ujp_write_terminate(struct ujp_buffer *buf)
{
	ujp_write_init(buf, -1, NULL, 0);
}

static LJ_AINLINE void write_reserve(struct ujp_buffer *buf, size_t n)
{
	if (LJ_UNLIKELY(!write_buffer_has(buf, n)))
		ujp_write_flush_buffer(buf);
}

/* Writes a byte to the output buffer. */
void ujp_write_byte(struct ujp_buffer *buf, uint8_t b)
{
	write_reserve(buf, sizeof(b));
	*buf->pos++ = b;
}

/* Writes an unsigned integer which is at most 64 bits long to the output. */
void ujp_write_u64(struct ujp_buffer *buf, uint64_t n)
{
	write_reserve(buf, LEB128_U64_MAXSIZE);
	buf->pos += (ptrdiff_t)write_uleb128(buf->pos, n);
}

/* Writes n bytes from an arbitrary buffer src to the output. */
static void write_buffer(struct ujp_buffer *buf, const void *src, size_t n)
{
	if (LJ_UNLIKELY(n > buf->size)) {
		/*
		 * Very unlikely: We are told to write a large buffer at once,
		 * and have to perform a syscall directly without buffering.
		 */
		ujp_write_flush_buffer(buf);
		write_buffer_sys(buf, src, n);
		return;
	}

	write_reserve(buf, n);
	write_memcpy(buf->pos, src, n);
	buf->pos += (ptrdiff_t)n;
}

/* Writes a \0-terminated C string to the output buffer. */
void ujp_write_string(struct ujp_buffer *buf, const char *s)
{
	size_t l = strlen(s);

	ujp_write_u64(buf, (uint64_t)l);
	write_buffer(buf, s, l);
}

/* Writes common part of the frame (frame-header and frame-id) to the output. */
static LJ_AINLINE void write_frame_common(struct ujp_buffer *buf,
					  uint8_t header, uint64_t id)
{
	ujp_write_byte(buf, header);
	ujp_write_u64(buf, id);
}

void ujp_write_main_lua(struct ujp_buffer *buf, uint8_t frame_type)
{
	write_set_frame_type(&frame_type, UJP_FRAME_TYPE_MAIN);
	write_frame_common(buf, frame_type, FRAME_ID_MAIN_LUA);
}

void ujp_write_marked_lfunc(struct ujp_buffer *buf, uint8_t frame_type,
			    uint64_t pt)
{
	write_set_frame_type(&frame_type, UJP_FRAME_TYPE_LFUNC);
	write_frame_common(buf, frame_type, (uint64_t)pt);
}

void ujp_write_new_lfunc(struct ujp_buffer *buf, uint8_t frame_type,
			 uint64_t pt, const char *name, uint64_t fl)
{
	write_set_frame_type(&frame_type, UJP_FRAME_TYPE_LFUNC);
	write_set_frame_flag(&frame_type, UJP_FRAME_EXPLICIT_SYMBOL);
	write_frame_common(buf, frame_type, pt);
	ujp_write_string(buf, name);
	ujp_write_u64(buf, fl);
}

void ujp_write_bottom_frame(struct ujp_buffer *buf)
{
	uint8_t type = UJP_FRAME_BOTTOM;

	write_set_frame_type(&type, UJP_FRAME_TYPE_MAIN);
	write_frame_common(buf, type, FRAME_ID_MAIN_LUA);
}

void ujp_write_cfunc(struct ujp_buffer *buf, uint8_t frame_type, uint64_t cf)
{
	write_set_frame_type(&frame_type, UJP_FRAME_TYPE_CFUNC);
	write_frame_common(buf, frame_type, cf);
}

void ujp_write_ffunc(struct ujp_buffer *buf, uint8_t frame_type, uint64_t ffid)
{
	write_set_frame_type(&frame_type, UJP_FRAME_TYPE_FFUNC);
	write_frame_common(buf, frame_type, ffid);
}

void ujp_write_hvmstate(struct ujp_buffer *buf, uint64_t addr)
{
	uint8_t frame_type = UJP_FRAME_FOR_LEAF_PROFILE;

	write_set_frame_type(&frame_type, UJP_FRAME_TYPE_HOST);
	write_frame_common(buf, frame_type, addr);
}

void ujp_write_new_trace(struct ujp_buffer *buf, uint8_t frame_type,
			 uint64_t generation, uint64_t traceno,
			 const char *name, uint64_t line)
{
	write_set_frame_type(&frame_type, UJP_FRAME_TYPE_TRACE);
	write_set_frame_flag(&frame_type, UJP_FRAME_EXPLICIT_SYMBOL);

	write_frame_common(buf, frame_type, (uint64_t)traceno);
	ujp_write_u64(buf, generation);
	ujp_write_string(buf, name);
	ujp_write_u64(buf, line);
}

void ujp_write_marked_trace(struct ujp_buffer *buf, uint8_t frame_type,
			    uint64_t generation, uint64_t traceno)
{
	write_set_frame_type(&frame_type, UJP_FRAME_TYPE_TRACE);
	write_frame_common(buf, frame_type, (uint64_t)traceno);
	ujp_write_u64(buf, generation);
}

int ujp_write_test_flag(const struct ujp_buffer *buf, uint8_t flag)
{
	return buf->flags & flag;
}
