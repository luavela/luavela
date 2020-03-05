/*
 * Stripped implementation of cityhash has been taken from
 * https://github.com/google/cityhash
 * https://github.com/google/cityhash/blob/master/src/city.cc
 *
 * Copyright (c) 2011 Google, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * CityHash, by Geoff Pike and Jyrki Alakuijala
 */

/*
 * It's implied that the hash function operability and quality is verified by
 * tests of the original project's tests and SMHasher testing suite which can
 * be found:
 * https://github.com/aappleby/smhasher
 * https://github.com/rurban/smhasher (extended suite)
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#if defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#define bswap_32(x) OSSwapInt32(x)
#else /* !defined(__APPLE__) */
#include <byteswap.h>
#endif /* defined(__APPLE__) */

static uint32_t UNALIGNED_LOAD32(const char *p)
{
	uint32_t result;

	memcpy(&result, p, sizeof(result));
	return result;
}

/*
 * NB! As our target architecture is Intel x86_64, it is _implied_ that
 * little endian is the _only_ possible byte order to be considered.
 * Original implementation contains endianness-related defines
 * (grep for uint32_t_in_expected_order ) which have been removed.
 */

static uint32_t Fetch32(const char *p)
{
	return UNALIGNED_LOAD32(p);
}

/* Magic numbers for 32-bit hashing. Copied from Murmur3. */
static const uint32_t c1 = 0xcc9e2d51;
static const uint32_t c2 = 0x1b873593;

/* A 32-bit to 32-bit integer hash copied from Murmur3. */
static uint32_t fmix(uint32_t h)
{
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}

static uint32_t Rotate32(uint32_t val, int shift)
{
	/* Avoid shifting by 32: doing so yields an undefined result. */
	return shift == 0 ? val : ((val >> shift) | (val << (32 - shift)));
}

/*
 * NB! The only C++ feature used in the original implementation is std::swap:
 *
 * #define PERMUTE3(a, b, c) do { std::swap(a, b); std::swap(a, c); } while (0)
 *
 * We use a naive implementation of swap, which kinda resembles std::swap from
 * /usr/include/c++/4.8/bits/move.h.
 */

static void swap(uint32_t *a, uint32_t *b)
{
	uint32_t tmp = *a;
	*a = *b;
	*b = tmp;
}

static void PERMUTE3(uint32_t a, uint32_t b, uint32_t c)
{
	swap(&a, &b);
	swap(&a, &c);
}

static uint32_t Mur(uint32_t a, uint32_t h)
{
	/* Helper from Murmur3 for combining two 32-bit values. */
	a *= c1;
	a = Rotate32(a, 17);
	a *= c2;
	h ^= a;
	h = Rotate32(h, 19);
	return h * 5 + 0xe6546b64;
}

static uint32_t Hash32Len13to24(const char *s, size_t len)
{
	uint32_t a = Fetch32(s - 4 + (len >> 1));
	uint32_t b = Fetch32(s + 4);
	uint32_t c = Fetch32(s + len - 8);
	uint32_t d = Fetch32(s + (len >> 1));
	uint32_t e = Fetch32(s);
	uint32_t f = Fetch32(s + len - 4);
	uint32_t h = len;

	return fmix(Mur(f, Mur(e, Mur(d, Mur(c, Mur(b, Mur(a, h)))))));
}

static uint32_t Hash32Len0to4(const char *s, size_t len)
{
	uint32_t b = 0;
	uint32_t c = 9;
	uint32_t i;

	for (i = 0; i < len; i++) {
		signed char v = s[i];

		b = b * c1 + v;
		c ^= b;
	}
	return fmix(Mur(b, Mur(len, c)));
}

static uint32_t Hash32Len5to12(const char *s, size_t len)
{
	uint32_t a = len, b = len * 5, c = 9, d = b;

	a += Fetch32(s);
	b += Fetch32(s + len - 4);
	c += Fetch32(s + ((len >> 1) & 4));
	return fmix(Mur(c, Mur(b, Mur(a, d))));
}

static uint32_t CityHash32(const char *s, size_t len)
{
	if (len <= 24) {
		return len <= 12 ? (len <= 4 ? Hash32Len0to4(s, len) :
					       Hash32Len5to12(s, len)) :
				   Hash32Len13to24(s, len);
	}

	/* len > 24 */
	uint32_t h = len, g = c1 * len, f = g;
	uint32_t a0 = Rotate32(Fetch32(s + len - 4) * c1, 17) * c2;
	uint32_t a1 = Rotate32(Fetch32(s + len - 8) * c1, 17) * c2;
	uint32_t a2 = Rotate32(Fetch32(s + len - 16) * c1, 17) * c2;
	uint32_t a3 = Rotate32(Fetch32(s + len - 12) * c1, 17) * c2;
	uint32_t a4 = Rotate32(Fetch32(s + len - 20) * c1, 17) * c2;

	h ^= a0;
	h = Rotate32(h, 19);
	h = h * 5 + 0xe6546b64;
	h ^= a2;
	h = Rotate32(h, 19);
	h = h * 5 + 0xe6546b64;
	g ^= a1;
	g = Rotate32(g, 19);
	g = g * 5 + 0xe6546b64;
	g ^= a3;
	g = Rotate32(g, 19);
	g = g * 5 + 0xe6546b64;
	f += a4;
	f = Rotate32(f, 19);
	f = f * 5 + 0xe6546b64;
	size_t iters = (len - 1) / 20;

	do {
		uint32_t a0 = Rotate32(Fetch32(s) * c1, 17) * c2;
		uint32_t a1 = Fetch32(s + 4);
		uint32_t a2 = Rotate32(Fetch32(s + 8) * c1, 17) * c2;
		uint32_t a3 = Rotate32(Fetch32(s + 12) * c1, 17) * c2;
		uint32_t a4 = Fetch32(s + 16);

		h ^= a0;
		h = Rotate32(h, 18);
		h = h * 5 + 0xe6546b64;
		f += a1;
		f = Rotate32(f, 19);
		f = f * c1;
		g += a2;
		g = Rotate32(g, 18);
		g = g * 5 + 0xe6546b64;
		h ^= a3 + a1;
		h = Rotate32(h, 19);
		h = h * 5 + 0xe6546b64;
		g ^= a4;
		g = bswap_32(g) * 5;
		h += a4 * 5;
		h = bswap_32(h);
		f += a0;
		PERMUTE3(f, h, g);
		s += 20;
	} while (--iters != 0);

	g = Rotate32(g, 11) * c1;
	g = Rotate32(g, 17) * c1;
	f = Rotate32(f, 11) * c1;
	f = Rotate32(f, 17) * c1;
	h = Rotate32(h + g, 19);
	h = h * 5 + 0xe6546b64;
	h = Rotate32(h, 17) * c1;
	h = Rotate32(h + f, 19);
	h = h * 5 + 0xe6546b64;
	h = Rotate32(h, 17) * c1;
	return h;
}

uint32_t strhash_city(const void *key, uint32_t len)
{
	return CityHash32(key, len);
}
