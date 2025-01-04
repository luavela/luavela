/*
 * Interfaces for working with LEB128/ULEB128 encoding.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJIT_UTILS_LEB128_H_
#define _UJIT_UTILS_LEB128_H_

#include <stddef.h>
#include <stdint.h>

/* Maximum number of bytes needed for LEB128 encoding of any 64-bit value. */
#define LEB128_U64_MAXSIZE 10

/* Writes a value from an unsigned 64-bit input to a buffer of bytes.
** Buffer overflow is not checked. Returns number of bytes written.
*/
size_t write_uleb128(uint8_t *buffer, uint64_t value);

/* Writes a value from an signed 64-bit input to a buffer of bytes.
** Buffer overflow is not checked. Returns number of bytes written.
*/
size_t write_leb128(uint8_t *buffer, int64_t value); /* aka sleb128. */

/* Reads a value from a buffer of bytes to a uint64_t output.
** Buffer overflow is not checked. Returns number of bytes read.
*/
size_t read_uleb128(uint64_t *out, const uint8_t *buffer);

/* Reads a value from a buffer of bytes to a int64_t output.
** Buffer overflow is not checked. Returns number of bytes read.
*/
size_t read_leb128(int64_t *out, const uint8_t *buffer); /* aka sleb128. */

/* Reads a value from a buffer of bytes to a uint64_t output. Consumes no more
** than n bytes. Buffer overflow is not checked. Returns number of bytes read.
** If more than n bytes is about to be consumed, returns 0 without touching out.
*/
size_t read_uleb128_n(uint64_t *out, const uint8_t *buffer, size_t n);

/* Reads a value from a buffer of bytes to a int64_t output. Consumes no more
** than n bytes. Buffer overflow is not checked. Returns number of bytes read.
** If more than n bytes is about to be consumed, returns 0 without touching out.
*/
size_t read_leb128_n(int64_t *out, const uint8_t *buffer, size_t n); /* aka sleb128. */

#endif /* !_UJIT_UTILS_LEB128_H_ */
