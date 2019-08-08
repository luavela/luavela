/*
 * The original hashing function used for strings in LuaJIT 2.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */
#include "utils/strhash.h"

/* Compute string hash. Constants taken from lookup3 hash by Bob Jenkins. */
uint32_t strhash_luajit2(const void *key, uint32_t len) {
  const char *str = key;
  uint32_t a, b, h = len;

  if (len >= 4) {  /* Caveat: unaligned access! */
    a = lj_getu32(str);
    h ^= lj_getu32(str+len-4);
    b = lj_getu32(str+(len>>1)-2);
    h ^= b; h -= lj_rol(b, 14);
    b += lj_getu32(str+(len>>2)-1);
  } else {
    a = *(const uint8_t *)str;
    h ^= *(const uint8_t *)(str+len-1);
    b = *(const uint8_t *)(str+(len>>1));
    h ^= b; h -= lj_rol(b, 14);
  }
  a ^= h; a -= lj_rol(h, 11);
  b ^= a; b -= lj_rol(a, 25);
  h ^= b; h -= lj_rol(b, 16);

  return h;
}


