/*
 * Prototype handling.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJ_PROTO_H
#define _UJ_PROTO_H

#include <stdio.h>

#include "lj_bcins.h"
#include "lj_obj.h"

/* Name of a chunk if it's passed through command line */
#define UJ_CMD_CHUNKNAME "=(command line)"

static LJ_AINLINE size_t uj_proto_sizeof(const GCproto *pt)
{
	return pt->sizept;
}

/* Saturating 3 bit counter (0..7) for created closures. */
void uj_proto_count_closure(GCproto *pt);

/* Free memory allocated for the prototype. */
void uj_proto_free(global_State *g, GCproto *pt);

/*
 * Returns true if prototype pt contains at least one bytecode that accesses a
 * global variable. Returns false otherwise.
 */
int uj_proto_accesses_globals(const GCproto *pt);

/*
 * Blacklist hotcounting byte code ins of the prototype pt replacing it with
 * its respective non-hot-counting counterpart.
 */
void uj_proto_blacklist_ins(GCproto *pt, BCIns *ins);

/*
 * Returns a non-0 value if JIT compilation is disabled for prototype pt,
 * and 0 otherwise.
 */
static LJ_AINLINE int uj_proto_jit_disabled(const GCproto *pt)
{
	return (int)(pt->flags & PROTO_NOJIT);
}

/*
 * Returns true if chunk originates from a string passed to command line
 * (ujit ... -e <chunk_source>)
 */
static LJ_AINLINE int uj_proto_is_cmdline_chunk(const GCproto *pt)
{
	return strcmp(proto_chunknamestr(pt), UJ_CMD_CHUNKNAME) == 0;
}

/*
 * Disable JIT compilation of the prototype. All hot-counting byte codes of the
 * prototype are replaced with their respective non-hot-counting counterparts.
 */
void uj_proto_disable_jit(GCproto *pt);

/*
 * Enable JIT compilation of the prototype. All non-hot-counting byte codes of
 * the prototype are replaced with their respective hot-counting counterparts.
 */
void uj_proto_enable_jit(GCproto *pt);

/* For a sealed prototype, propagates the seal mark to dependent objects. */
void uj_proto_seal_traverse(lua_State *L, GCproto *pt, gco_mark_flipper marker);

/* Deep copy of prototype with zero upvalues to possibly another global state */
GCproto *uj_proto_deepcopy(lua_State *L, const GCproto *pt,
			   struct deepcopy_ctx *ctx);

/* Get line number for a bytecode position. */
BCLine uj_proto_line(const GCproto *pt, BCPos pos);

/* Get name of upvalue. */
const char *uj_proto_uvname(const GCproto *pt, uint32_t idx);

/* Get name of a local variable from a bytecode position and a slot number. */
const char *uj_proto_varname(const GCproto *pt, BCPos pos, BCReg slot);

/* Fixed internal variable names: */
#define VARNAMEDEF(_)                 \
	_(FOR_IDX, "(for index)")     \
	_(FOR_STOP, "(for limit)")    \
	_(FOR_STEP, "(for step)")     \
	_(FOR_GEN, "(for generator)") \
	_(FOR_STATE, "(for state)")   \
	_(FOR_CTL, "(for control)")

enum {
	/* sentinel */
	VARNAME_END,
#define VARNAMEENUM(name, str) VARNAME_##name,
	VARNAMEDEF(VARNAMEENUM)
#undef VARNAMEENUM
	/* sentinel */
	VARNAME__MAX
};

#define FORMATTED_LOC_BUF_SIZE 80

/*
 * Pretty-prints location of pc-th bytecode in proto's bytecode array and
 * stores the result in the buffer loc_buf. Guarantees to print no more than
 * FORMATTED_LOC_BUF_SIZE characters to the buffer.
 */
void uj_proto_sprintloc(char *loc_buf, const GCproto *pt, BCPos pos);

/*
 * Pretty-prints location of pc-th bytecode in proto's bytecode array to the
 * output.
 */
void uj_proto_fprintloc(FILE *out, const GCproto *pt, BCPos pos);

/* Hack for the lexer which is a de-facto constructor of prototypes. */
void uj_proto_chunknamencpy(char *out, const GCstr *str, size_t n);

/*
 * Generate shortened source name.
 * Writes the result to out of size n bytes, ensuring \0-termination of out.
 */
static LJ_AINLINE void uj_proto_namencpy(char *out, const GCproto *pt, size_t n)
{
	uj_proto_chunknamencpy(out, proto_chunkname(pt), n);
}

#endif /* !_UJ_PROTO_H */
