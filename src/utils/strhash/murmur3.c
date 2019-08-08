/*
 * Stripped implementation of murmur3 taken from
 * https://code.google.com/p/smhasher/
 */

//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

// Note - The x86 and x64 versions do _not_ produce the same results, as the
// algorithms are optimized for their respective platforms. You can still
// compile and run any of them on any platform, but your performance with the
// non-native version will be less than optimal.

//-----------------------------------------------------------------------------
// Platform-specific functions and macros

#include "utils/strhash.h"

#define MURMUR3_SEED (0xdeadbeef)

//-----------------------------------------------------------------------------
// Finalization mix - force all bits of a hash block to avalanche

static LJ_AINLINE uint32_t fmix32(uint32_t h) {
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;

  return h;
}

//-----------------------------------------------------------------------------

static uint32_t _MurmurHash3_x86_32(const void *key, uint32_t len, uint32_t seed) {
  const uint8_t *data = (const uint8_t *)key;
  const uint32_t nblocks = (len >> 2);
  uint32_t i;


  uint32_t h1 = seed;
  uint32_t k1 = 0;

  const uint32_t c1 = 0xcc9e2d51;
  const uint32_t c2 = 0x1b873593;


  const uint8_t *tail = (const uint8_t *)(data + (nblocks << 2));

  //----------
  // body

  for(i = 0; i < nblocks; i++) {
    uint32_t k1 = lj_getu32(data + (i << 2));

    k1 *= c1;
    k1 = lj_rol(k1,15);
    k1 *= c2;

    h1 ^= k1;
    h1 = lj_rol(h1,13);
    h1 = h1*5+0xe6546b64;
  }

  //----------
  // tail

  switch(len & 3) {
  case 3: k1 ^= tail[2] << 16;
  case 2: k1 ^= tail[1] << 8;
  case 1: k1 ^= tail[0];
          k1 *= c1; k1 = lj_rol(k1,15); k1 *= c2; h1 ^= k1;
  };

  //----------
  // finalization

  h1 ^= len;

  h1 = fmix32(h1);

  return h1;
}

uint32_t strhash_murmur3(const void *key, uint32_t len) {
  return _MurmurHash3_x86_32(key, len, MURMUR3_SEED);
}

