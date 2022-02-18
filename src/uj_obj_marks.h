/*
 * Definitions of object marks (GC-related as well as other attributes).
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJ_OBJ_MARKS_H
#define _UJ_OBJ_MARKS_H

#include "lj_def.h"

/*
 * All collectable objects have a dedicated field that stores various marks,
 * each mark representing a particular property of an object. Some marks are
 * related to GC, some others represent GC-independent properties of an
 * object. Currently all marks are packed in 8 bits and overlap:
 *
 *         MSB                                                     <-LSB-<
 * +----------------------------------------------------------------------+
 * |\\\\\|  7   |    6    |   5   |    4    |    3    |  2  |  1   |  0   |
 * +----------------------------------------------------------------------+
 * |GC:  |      |         | FIXED | WEAKVAL | WEAKKEY |BLACK|WHITE1|WHITE0|
 * |GC:  |CD_VAR|         |       |CDATA_FIN|FINALIZED|     |      |      |
 * +---------------------------------------------------------- -----------+
 * |GCO: |SEALED|IMMUTABLE|TMPMARK|         |         |     |      |      |
 * +----------------------------------------------------------------------+
 *
 */

#define LJ_GC_WHITE0 0x01
#define LJ_GC_WHITE1 0x02
#define LJ_GC_BLACK 0x04
#define LJ_GC_FINALIZED 0x08
#define LJ_GC_WEAKKEY 0x08
#define LJ_GC_WEAKVAL 0x10
#define LJ_GC_CDATA_FIN 0x10
#define LJ_GC_FIXED 0x20
#define LJ_GC_CDATA_VAR 0x80

#define UJ_GCO_TMPMARK 0x20
#define UJ_GCO_IMMUTABLE 0x40
#define UJ_GCO_SEALED 0x80

/*
 * NB! Implementation limitation: marks for var-length cdata and sealed
 * objects overlap. Fortunately, we do not support sealing for cdata.
 * But something must be fixed here if one day we decide to.
 */
LJ_STATIC_ASSERT(LJ_GC_CDATA_VAR == UJ_GCO_SEALED);

LJ_STATIC_ASSERT(LJ_GC_FIXED == UJ_GCO_TMPMARK);

#endif /* !_UJ_OBJ_MARKS_H */
