/*
 * Working with LEB128/ULEB128 encoding.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <stdint.h>
#include <stddef.h>

#define LINK_BIT          (0x80)
#define MIN_TWOBYTE_VALUE (0x80)
#define PAYLOAD_MASK      (0x7f)
#define SHIFT_STEP        (7)
#define LEB_SIGN_BIT      (0x40)

/* ------------------------- Writing ULEB128/LEB128 ------------------------- */

size_t write_uleb128(uint8_t *buffer, uint64_t value) {
  size_t i = 0;

  for (; value >= MIN_TWOBYTE_VALUE; value >>= SHIFT_STEP) {
    buffer[i++] = (uint8_t)((value & PAYLOAD_MASK) | LINK_BIT);
  }
  buffer[i++] = (uint8_t)value;

  return i;
}

size_t write_leb128(uint8_t *buffer, int64_t value) {
  size_t i = 0;

  for (; (uint64_t)(value + 0x40) >= MIN_TWOBYTE_VALUE; value >>= SHIFT_STEP) {
    buffer[i++] = (uint8_t)((value & PAYLOAD_MASK) | LINK_BIT);
  }
  buffer[i++] = (uint8_t)(value & PAYLOAD_MASK);

  return i;
}

/* ------------------------- Reading ULEB128/LEB128 ------------------------- */

/* NB! For each LEB128 type (signed/unsigned) we have two versions of read
** functions: The one consuming unlimited number of input octets and the one
** consuming not more than given number of input octets. Currently reading is not
** used in performance critical places, so these two functions are implemented
** via single low-level function + run-time mode check. Feel free to change if
** this becomes a bottleneck.
*/

size_t _read_uleb128(uint64_t *out, const uint8_t *buffer, int guarded, size_t n) {
  size_t i = 0;
  uint64_t value = 0;
  uint64_t shift = 0;

  for(;;) {
    if (guarded && i + 1 > n) {
      return 0;
    }
    uint8_t octet = buffer[i++];
    value |= ((uint64_t)(octet & PAYLOAD_MASK)) << shift;
    shift += SHIFT_STEP;
    if (!(octet & LINK_BIT)) {
      break;
    }
  }

  *out = value;
  return i;
}

size_t read_uleb128(uint64_t *out, const uint8_t *buffer) {
  return _read_uleb128(out, buffer, 0, 0);
}

size_t read_uleb128_n(uint64_t *out, const uint8_t *buffer, size_t n) {
  return _read_uleb128(out, buffer, 1, n);
}

static size_t _read_leb128(int64_t *out, const uint8_t *buffer, int guarded, size_t n) {
  size_t i = 0;
  int64_t  value = 0;
  uint64_t shift = 0;
  uint8_t  octet;

  for(;;) {
    if (guarded && i + 1 > n) {
      return 0;
    }
    octet  = buffer[i++];
    value |= ((int64_t)(octet & PAYLOAD_MASK)) << shift;
    shift += SHIFT_STEP;
    if (!(octet & LINK_BIT)) {
      break;
    }
  }

  if (octet & LEB_SIGN_BIT && shift < sizeof(int64_t) * 8) {
    value |= -(1 << shift);
  }

  *out = value;
  return i;
}

size_t read_leb128(int64_t *out, const uint8_t *buffer) {
  return _read_leb128(out, buffer, 0, 0);
}

size_t read_leb128_n(int64_t *out, const uint8_t *buffer, size_t n) {
  return _read_leb128(out, buffer, 1, n);
}
