/*
 * String scanning.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _UJIT_UTILS_STRSCAN_H
#define _UJIT_UTILS_STRSCAN_H

/* Options for accepted/returned formats. */
#define STRSCAN_OPT_TONUM 0x01  /* Always convert to double. */
#define STRSCAN_OPT_IMAG  0x02  /* Support I/i suffix for imaginary numbers. */
#define STRSCAN_OPT_LL    0x04  /* Convert to signed/unsigned long long. */
#define STRSCAN_OPT_C     0x08  /* Convert to signed/unsiged int. */

/* Desribes conversion status and format of the output. */
typedef enum {
  STRSCAN_ERROR, /* Error during conversion. All other values denote successful conversion. */
  STRSCAN_NUM,   /* Output is double. */
  STRSCAN_IMAG,  /* Output is double, but the input was warked with I/i suffix. */
  STRSCAN_INT,   /* Output is  int32_t, stored in lower 4 bytes of the output buffer. */
  STRSCAN_U32,   /* Output is uint32_t, stored in lower 4 bytes of the output buffer. */
  STRSCAN_I64,   /* Output is  int64_t. */
  STRSCAN_U64,   /* Output is uint64_t. */
} StrScanFmt;

StrScanFmt strscan_tonumber(const uint8_t *p, double *d, uint32_t opt);

#endif /* !_UJIT_UTILS_STRSCAN_H */
