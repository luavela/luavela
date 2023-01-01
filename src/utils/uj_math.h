/*
 * Math library.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _UJ_MATH_H_
#define _UJ_MATH_H_

#include "lj_def.h"
#include "uj_arch.h"

int32_t uj_math_modi(int32_t a, int32_t b);
double uj_math_powi(double x, int exp);

/*
 * Simple binary(mostly) calculator for doubles.
 * Works on different sets of operation encodings within uJIT, namely:
 * - IR operations in narrow and fold optimizations.
 * - Metamethods for numeric types when string coercion is performed.
 * - Parser operation tokens when parsing something like 'local a = 5 + 7'.
 *
 * Since the algorithm works on different encodings, it is merely a generic
 * algorithm (something like std::sort in C++). Like any generic algorithm,
 * it applies some restrictions to the underlying data, but since C does not
 * provide established interface mechanism, we only _ask_ all encodings that
 * want to use this algorithm the following:
 * - Encoding should implement a subset of described operations (possibly not
 *   all of them). For example, parser does not have atan2 token since it's
 *   a function.
 * - Relative positions of operations in the encoding (as passed to the
 *   algorithm) should be as this function requires. For example, if OP_ADD
 *   and OP_SUB are defined, OP_SUB should be 1 plus OP_ADD. This will allow
 *   to call uj_math_foldarith directly like (x, y, op-OP_ADD). This restriction
 *   is historically called ORDER ARITH and is found here and there in the
 *   sources. It might actually be violated by converting source operations
 *   to ORDER ARITH explicitly, but it's considered a bad practice.
 */
enum FoldarithOp {
	FoldarithAdd,
	FoldarithSub,
	FoldarithMul,
	FoldarithDiv,
	FoldarithMod,
	FoldarithPow,
	FoldarithNeg,
	FoldarithAbs,
	FoldarithAtan2,
	FoldarithLdexp,
	FoldarithMin,
	FoldarithMax,
	FoldarithLast
};

double uj_math_foldarith(double x, double y, enum FoldarithOp op);

/*
 * Advanced unary calculator for doubles.
 * Only used in IR calculations, so underlying array of IR operations
 * must comply to the order of FoldfpmOp.
 */
enum FoldfpmOp {
	FoldfpmFloor,
	FoldfpmCeil,
	FoldfpmTrunc,
	FoldfpmSqrt,
	FoldfpmExp,
	FoldfpmExp2,
	FoldfpmLog,
	FoldfpmLog2,
	FoldfpmLog10,
	FoldfpmSin,
	FoldfpmCos,
	FoldfpmTan,
	FoldfpmLast
};

double uj_math_foldfpm(double x, enum FoldfpmOp op);

#endif /* !_UJ_MATH_H_ */
