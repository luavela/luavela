/*
 * Interfaces for JIT hotcounting.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJ_HOTCNT_H
#define _UJ_HOTCNT_H

#include <inttypes.h>

#include "lj_def.h"
#include "lj_bcins.h"

/* Absolute offsets between HOTCNT and hotcounting instructions */
#define ITERL_OFFSET 2
#define PROLOGUE_OFFSET 1
#define LOOP_FORL_OFFSET 1

/* Offset in bytes to HOTCNT's counter */
#define COUNTER_OFFSET 2

struct global_State;
struct GCproto;

/* Iterate all prototypes and reset counters */
void uj_hotcnt_patch_protos(global_State *g);
/* Set adjusted counter value to the given instruction */
void uj_hotcnt_set_counter(BCIns *bc, uint16_t val);
/* Patch bytecode for given GCproto */
void uj_hotcnt_patch_bc(GCproto *pt, uint16_t hotloop);

#endif /* !_UJ_HOTCNT_H */
