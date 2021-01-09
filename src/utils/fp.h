/*
 * Utilities for working with floating point number.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _LJ_UTILS_FP_H
#define _LJ_UTILS_FP_H

#include <math.h>

#include "lj_def.h"

/* Generic union for double values conversion.
** Allows direct access to double encoding or
** parts of it.
*/
typedef union FpConv {
  double d;
  uint64_t u;
  struct {
    union {
     uint32_t lo;
     int32_t i;
    };
    uint32_t hi;
  };
} FpConv;

/* NB! About naming of constants: As per IEEE 754 -inf and +inf are defined
** unambiguously, whereas NaN can have a range of representations, and uJIT uses
** only a single value from this range in its math. However, all constants are
** named with the LJ_ prefix as they are extracted from lj_obj.h as is.
** Subject to change (migrate to libc?) as we are cleaning the math library.
*/
#define LJ_PINFINITY U64x(7ff00000, 00000000)
#define LJ_MINFINITY U64x(fff00000, 00000000)
#define LJ_NAN       U64x(fff80000, 00000000)

#define LJ_SIGN_MASK  U64x(80000000,00000000)
#define LJ_SIGN_MASK_INVERTED U64x(7fffffff,ffffffff)

/*
 * If exponent is all ones, the number is either +-inf or NaN:
 *     isfinite = (n & LJ_NFIN_MASK != LJ_NFIN_MASK);
 */
#define LJ_NFIN_MASK  U64x(7ff00000,00000000)

static LJ_AINLINE int32_t lj_num2bit(double n)
{
  /*
   * Here we behave differently depending on floating point environment
   * rounding mode. But this is ok as bit functions are declared to have
   * implementation-defined rounding for non-integral arguments.
   */
  return lrint(n);
}

/* Deducible types of IEEE754 floating point values.
** Only those of interest are enumerated.
*/
typedef enum {
  LJ_FP_FINITE,
  LJ_FP_NAN,
  LJ_FP_PINF,
  LJ_FP_MINF
} FpType;

/* Deduces floating point value type based on its value.
*/
FpType uj_fp_classify(double n);

#define lj_fp_finite(n) (uj_fp_classify(n) == LJ_FP_FINITE)

#define lj_num2int(n)   ((int32_t)(n))
#define lj_num2u64(n)   ((uint64_t)(n))

#endif /* !_LJ_UTILS_FP_H */
