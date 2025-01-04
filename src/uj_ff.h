/*
 * Fast function IDs.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _UJ_FF_H
#define _UJ_FF_H

#include "uj_funcid.h"

/* Fast function IDs */
enum fast_func {
	FF_LUA_ = FF_LUA,
	FF_C_ = FF_C,
#define FFDEF(name) FF_##name,
#include "lj_ffdef.h"
#undef FFDEF
	FF__MAX
};

#endif /* !_UJ_FF_H */
