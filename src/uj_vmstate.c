/*
 * Data related to uJIT VM states.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "lj_def.h"
#include "uj_vmstate.h"

LJ_DATADEF const char *const uj_vmstate_names[] = {
#define VMSTATENAME(name) (#name),
	VMSTATEDEF(VMSTATENAME)
#undef VMSTATENAME
	/* sentinel */
	NULL};
