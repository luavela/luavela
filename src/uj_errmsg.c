/*
 * Implementation of aux routines for error codes and messages.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "uj_str.h"
#include "uj_errmsg.h"

#pragma GCC diagnostic ignored "-Woverlength-strings"
/* uj_errmsgs becomes too long for ISO C99 */

LJ_DATADEF const char *uj_errmsgs =
#define ERRDEF(name, msg) msg "\0"
#include "uj_errors.h"
	;

LJ_NOINLINE GCstr *uj_errmsg_str(struct lua_State *L, enum err_msg em)
{
	return uj_str_newz(L, uj_errmsg(em));
}
