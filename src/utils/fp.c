/*
 * Utilities for working with floating point number.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "utils/fp.h"

FpType uj_fp_classify(double n)
{
	FpConv c;
	c.d = n;
	if (LJ_LIKELY((c.u << 1) < 0xffe0000000000000))
		return LJ_FP_FINITE;
	if ((c.u & 0x000fffffffffffff) != 0)
		return LJ_FP_NAN; /* sNaN or qNaN. */
	if ((c.u & 0x8000000000000000) == 0)
		return LJ_FP_PINF;
	return LJ_FP_MINF;
}

