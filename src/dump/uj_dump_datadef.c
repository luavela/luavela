/*
 * Data definitions for C-level BC / IR / mcode dumpers.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "lj_bc.h"
#include "jit/lj_ircall.h"

/* Platform-specific data definitions */

#include "dump/uj_dump_datadef_regs_x86.h"

LJ_DATADEF const char *const dump_bc_names[] = {
#define BCNAME(name, ma, mb, mc, mt) (#name),
	BCDEF(BCNAME)
#undef BCNAME
	/* sentinel */
	NULL};

LJ_DATADEF const char *const dump_ir_names[] = {
#define IRNAMES(name, m, m1, m2) (#name),
	IRDEF(IRNAMES)
#undef IRNAMES
};

LJ_DATADEF const char *const dump_ff_names[] = {
	"lua_function",
	"c_function",
#define FFDEF(name) (#name),
#include "lj_ffdef.h"
#undef FFDEF
};

LJ_DATADEF const char *const dump_ir_types[] = {
	"nil", "fal", "tru", "lud", "str", "p32", "thr", "pro",
	"fun", "p64", "cdt", "tab", "udt", "flt", "num", "i8 ",
	"u8 ", "i16", "u16", "int", "u32", "i64", "u64", "tvl"};

LJ_DATADEF const char *const dump_ir_call_names[] = {
#define IRCALLNAME(cond, name, nargs, kind, type, flags) (#name),
	IRCALLDEF(IRCALLNAME)
#undef IRCALLNAME
	/* sentinel */
	NULL};

LJ_DATADEF const char *const dump_ir_field_names[] = {
#define FLNAME(name, ofs) (#name),
	IRFLDEF(FLNAME)
#undef FLNAME
	/* sentinel */
	NULL};

LJ_DATADEF const char *const dump_ir_fpm_names[] = {
#define FPMNAME(name) (#name),
	IRFPMDEF(FPMNAME)
#undef FPMNAME
	/* sentinel */
	NULL};

LJ_DATADEF const char *const dump_trace_errors[] = {
#define TREDEF(name, msg) (msg),
#include "jit/lj_traceerr.h"
#undef TREDEF
	/* sentinel */
	NULL};

/* Names of link types. ORDER LJ_TRLINK */
LJ_DATADEF const char *const dump_trace_lt_names[] = {
	"none",		"root",		  "loop",	"tail-recursion",
	"up-recursion", "down-recursion", "interpreter", "return"};

LJ_DATADEF const char *const dump_progress_state_names[] = {
#define DUMP_PROGRESS_DEF(state, suffix) (#suffix),
#include "dump/uj_dump_progress.h"
#undef DUMP_PROGRESS_DEF
};
