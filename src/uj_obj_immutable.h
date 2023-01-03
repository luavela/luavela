/*
 * Interfaces for making objects immutable.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJ_OBJ_IMMUTABLE_H
#define _UJ_OBJ_IMMUTABLE_H

#include "lj_def.h"
#include "lj_obj.h"
#include "uj_obj_marks.h"

/* Sets an immutability mark on the object. */
static LJ_AINLINE void uj_obj_immutable_set_mark(GCobj *o)
{
	o->gch.marked |= UJ_GCO_IMMUTABLE;
}

/*
 * Recursively makes target object and its contents immutable. If fails,
 * ensures that the object's state is intact and throws a run-time error.
 */
void uj_obj_immutable(lua_State *L, GCobj *o);

#endif /* !_UJ_OBJ_IMMUTABLE_H */
