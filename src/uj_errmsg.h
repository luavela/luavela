/*
 * Aux routines for error codes and messages.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJ_ERRMSG_H
#define _UJ_ERRMSG_H

#include "lj_def.h"

struct lua_State;

enum err_msg {
#define ERRDEF(name, msg) \
	UJ_ERR_##name, UJ_ERR_##name##_ = UJ_ERR_##name + sizeof(msg) - 1,
#include "uj_errors.h"
	UJ_ERR__MAX
};

/* Error message strings. */
LJ_DATA const char *uj_errmsgs;

static LJ_AINLINE const char *uj_errmsg(enum err_msg em)
{
	return uj_errmsgs + (ptrdiff_t)em;
}

/* Return string object for error message. */
GCstr *uj_errmsg_str(struct lua_State *L, enum err_msg em);

#endif /* !_UJ_ERRMSG_H */
