/*
 * Interfaces for sealing and unsealing objects.
 * A sealed object has following properties:
 *  * It is not garbage collected (this is achieved by ensuring that a sealed
 *    object links only to another sealed object or NULL in an object chain).
 *  * It is immutable.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */
#ifndef _UJ_OBJ_SEAL_H
#define _UJ_OBJ_SEAL_H

#include "lj_def.h"

struct global_State;
struct lua_State;
union GCobj;

/*
 * Recursively seals target object and its contents. If sealing succeeds,
 * restores GC seal invariant. Otherwise ensures that the object is left
 * in the same state as it was before the sealing attempt and throws a
 * run-time error.
 */
void uj_obj_seal(lua_State *L, GCobj *o);

/* Unseals all sealed objects at once. */
void uj_obj_unseal_all(global_State *g);

#endif /* !_UJ_OBJ_SEAL_H */
