/*
 * Hash functions for strings.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _LJ_UTILS_STRHASH_H
#define _LJ_UTILS_STRHASH_H

#include "lj_def.h"

/* To be exported via public API when needed: */
typedef uint32_t (*strhash_f)(const void *key, uint32_t len);

uint32_t strhash_murmur3(const void *key, uint32_t len);
uint32_t strhash_city(const void *key, uint32_t len);
uint32_t strhash_luajit2(const void *key, uint32_t len) UNUSED_FUNC;

#endif /* !_LJ_UTILS_STRHASH_H */
