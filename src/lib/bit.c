/*
 * Bit manipulation library.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "lua.h"
#include "lauxlib.h"
#include "lextlib.h"

#include "lj_obj.h"
#include "uj_lib.h"

/* ------------------------------------------------------------------------ */

#define LJLIB_MODULE_bit

LJLIB_ASM(bit_tobit)            LJLIB_REC(.)
{
  uj_lib_checknum(L, 1);
  return FFH_RETRY;
}
LJLIB_ASM_(bit_bnot)            LJLIB_REC(.)
LJLIB_ASM_(bit_bswap)           LJLIB_REC(.)

LJLIB_ASM(bit_lshift)           LJLIB_REC(.)
{
  uj_lib_checknum(L, 1);
  uj_lib_checkbit(L, 2);
  return FFH_RETRY;
}
LJLIB_ASM_(bit_rshift)          LJLIB_REC(.)
LJLIB_ASM_(bit_arshift)         LJLIB_REC(.)
LJLIB_ASM_(bit_rol)             LJLIB_REC(.)
LJLIB_ASM_(bit_ror)             LJLIB_REC(.)

LJLIB_ASM(bit_band)             LJLIB_REC(.)
{
  int i = 0;
  do { uj_lib_checknum(L, ++i); } while (L->base+i < L->top);
  return FFH_RETRY;
}
LJLIB_ASM_(bit_bor)             LJLIB_REC(.)
LJLIB_ASM_(bit_bxor)            LJLIB_REC(.)

/* ------------------------------------------------------------------------ */

LJLIB_CF(bit_tohex)
{
  uint32_t b = (uint32_t)uj_lib_checkbit(L, 1);
  int32_t i, n = L->base+1 >= L->top ? 8 : uj_lib_checkbit(L, 2);
  const char *hexdigits = "0123456789abcdef";
  char buf[8];
  if (n < 0) { n = -n; hexdigits = "0123456789ABCDEF"; }
  if (n > 8) n = 8;
  for (i = n; --i >= 0; ) { buf[i] = hexdigits[b & 15]; b >>= 4; }
  lua_pushlstring(L, buf, (size_t)n);
  return 1;
}

/* ------------------------------------------------------------------------ */

#include "lj_libdef.h"

LUALIB_API int luaopen_bit(lua_State *L)
{
  LJ_LIB_REG(L, LUAE_BITLIBNAME, bit);
  return 1;
}

