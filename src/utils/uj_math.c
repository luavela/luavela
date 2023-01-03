/*
 * Math library functions for JIT.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include <math.h>

#include "utils/uj_math.h"

#if LJ_HASJIT
int32_t uj_math_modi(int32_t a, int32_t b)
{
	uint32_t y, ua, ub;
	/* This must be checked before using this function. */
	lua_assert(b != 0);
	ua = a < 0 ? (uint32_t)-a : (uint32_t)a;
	ub = b < 0 ? (uint32_t)-b : (uint32_t)b;
	y = ua % ub;
	if (y != 0 && (a ^ b) < 0)
		y = y - ub;
	if (((int32_t)y ^ b) < 0)
		y = (uint32_t)0 - (int32_t)y;
	return (int32_t)y;
}

double uj_math_powi(double x, int exp)
{
	return pow(x, exp);
}
#endif /* LJ_HASJIT */

double uj_math_foldarith(double x, double y, enum FoldarithOp op)
{
	switch (op) {
	case FoldarithAdd:
		return (x + y);
		break;
	case FoldarithSub:
		return (x - y);
		break;
	case FoldarithMul:
		return (x * y);
		break;
	case FoldarithDiv:
		return (x / y);
		break;
	case FoldarithMod:
		return x - floor(x / y) * y;
		break;
	case FoldarithPow:
		return pow(x, y);
		break;
	case FoldarithNeg:
		return (-x);
		break;
	case FoldarithAbs:
		return fabs(x);
		break;
	case FoldarithAtan2:
		return atan2(x, y);
		break;
	case FoldarithLdexp:
		lua_assert(y == (int)y);
		return ldexp(x, (int)y);
		break;
	case FoldarithMin:
		return fmin(x, y);
		break;
	case FoldarithMax:
		return fmax(x, y);
		break;
	default:
		lua_assert(0);
		return 0;
		break;
	}
}

double uj_math_foldfpm(double x, enum FoldfpmOp op)
{
	switch (op) {
	case FoldfpmFloor:
		return floor(x);
		break;
	case FoldfpmCeil:
		return ceil(x);
		break;
	case FoldfpmTrunc:
		return trunc(x);
		break;
	case FoldfpmSqrt:
		return sqrt(x);
		break;
	case FoldfpmExp:
		return exp(x);
		break;
	case FoldfpmExp2:
		return exp2(x);
		break;
	case FoldfpmLog:
		return log(x);
		break;
	case FoldfpmLog2:
		return log2(x);
		break;
	case FoldfpmLog10:
		return log10(x);
		break;
	case FoldfpmSin:
		return sin(x);
		break;
	case FoldfpmCos:
		return cos(x);
		break;
	case FoldfpmTan:
		return tan(x);
		break;
	default:
		lua_assert(0);
		return 0;
		break;
	}
}
