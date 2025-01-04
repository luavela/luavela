/*
 * CRC32 calculation.
 *
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJ_CRC_H
#define _UJ_CRC_H

#include <stdint.h>

/*
 * Calculates CRC-32-IEEE 802.3 variation of CRC checksum for
 * null-terminated string
 */
uint32_t uj_crc32(const char *msg);

#endif /* !_UJ_CRC_H */
