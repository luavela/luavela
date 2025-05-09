/*
 * Fast x86 instruction length decoder.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 * Originally contributed to LuaJIT by Peter Cawley.
 */

#include "utils/x86_inslen.h"

static const uint8_t map_op1[256] = {
0x92,0x92,0x92,0x92,0x52,0x45,0x51,0x51,0x92,0x92,0x92,0x92,0x52,0x45,0x51,0x20,
0x92,0x92,0x92,0x92,0x52,0x45,0x51,0x51,0x92,0x92,0x92,0x92,0x52,0x45,0x51,0x51,
0x92,0x92,0x92,0x92,0x52,0x45,0x10,0x51,0x92,0x92,0x92,0x92,0x52,0x45,0x10,0x51,
0x92,0x92,0x92,0x92,0x52,0x45,0x10,0x51,0x92,0x92,0x92,0x92,0x52,0x45,0x10,0x51,
0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x14,0x14,0x14,0x14,0x14,0x14,0x14,0x14,
0x51,0x51,0x51,0x51,0x51,0x51,0x51,0x51,0x51,0x51,0x51,0x51,0x51,0x51,0x51,0x51,
0x51,0x51,0x92,0x92,0x10,0x10,0x12,0x11,0x45,0x86,0x52,0x93,0x51,0x51,0x51,0x51,
0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52,
0x93,0x86,0x93,0x93,0x92,0x92,0x92,0x92,0x92,0x92,0x92,0x92,0x92,0x92,0x92,0x92,
0x51,0x51,0x51,0x51,0x51,0x51,0x51,0x51,0x51,0x51,0x47,0x51,0x51,0x51,0x51,0x51,
0x59,0x59,0x59,0x59,0x51,0x51,0x51,0x51,0x52,0x45,0x51,0x51,0x51,0x51,0x51,0x51,
0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,
0x93,0x93,0x53,0x51,0x70,0x71,0x93,0x86,0x54,0x51,0x53,0x51,0x51,0x52,0x51,0x51,
0x92,0x92,0x92,0x92,0x52,0x52,0x51,0x51,0x92,0x92,0x92,0x92,0x92,0x92,0x92,0x92,
0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x45,0x45,0x47,0x52,0x51,0x51,0x51,0x51,
0x10,0x51,0x10,0x10,0x51,0x51,0x63,0x66,0x51,0x51,0x51,0x51,0x51,0x51,0x92,0x92
};

static const uint8_t map_op2[256] = {
0x93,0x93,0x93,0x93,0x52,0x52,0x52,0x52,0x52,0x52,0x51,0x52,0x51,0x93,0x52,0x94,
0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,
0x53,0x53,0x53,0x53,0x53,0x53,0x53,0x53,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,
0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x34,0x51,0x35,0x51,0x51,0x51,0x51,0x51,
0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,
0x53,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,
0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,
0x94,0x54,0x54,0x54,0x93,0x93,0x93,0x52,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,
0x46,0x46,0x46,0x46,0x46,0x46,0x46,0x46,0x46,0x46,0x46,0x46,0x46,0x46,0x46,0x46,
0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,
0x52,0x52,0x52,0x93,0x94,0x93,0x51,0x51,0x52,0x52,0x52,0x93,0x94,0x93,0x93,0x93,
0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x94,0x93,0x93,0x93,0x93,0x93,
0x93,0x93,0x94,0x93,0x94,0x94,0x94,0x93,0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52,
0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,
0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,
0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x52
};

uint32_t x86_inslen(const uint8_t* p)
{
  uint32_t result = 0;
  uint32_t prefixes = 0;
  uint32_t x = map_op1[*p];
  for (;;) {
    switch (x >> 4) {
    case 0: return result + x + (prefixes & 4);
    case 1: prefixes |= x; x = map_op1[*++p]; result++; break;
    case 2: x = map_op2[*++p]; break;
    case 3: p++; goto mrm;
    case 4: result -= (prefixes & 2);  /* fallthrough */
    case 5: return result + (x & 15);
    case 6:  /* Group 3. */
      if (p[1] & 0x38) x = 2;
      else if ((prefixes & 2) && (x == 0x66)) x = 4;
      goto mrm;
    case 7: /* VEX c4/c5. */
      if (x == 0x70) {
	x = *++p & 0x1f;
	result++;
	if (x >= 2) {
	  p += 2;
	  result += 2;
	  goto mrm;
	}
      }
      p++;
      result++;
      x = map_op2[*++p];
      break;
    case 8: result -= (prefixes & 2);  /* fallthrough */
    case 9: mrm:  /* ModR/M and possibly SIB. */
      result += (x & 15);
      x = *++p;
      switch (x >> 6) {
      case 0: if ((x & 7) == 5) return result + 4; break;
      case 1: result++; break;
      case 2: result += 4; break;
      case 3: return result;
      }
      if ((x & 7) == 4) {
	result++;
	if (x < 0x40 && (p[1] & 7) == 5) result += 4;
      }
      return result;
    }
  }
}


