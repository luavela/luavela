/*
 * Interfaces for C++ symbols demangling.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJPP_DEMANGLE_H
#define _UJPP_DEMANGLE_H

#include <inttypes.h>

#include "ujpp_hash.h"
#include "ujpp_vector.h"
#include "ujpp_utils.h"

struct parser_state;
struct hvmstate;

struct demangle_info {
	uint64_t so_idx; /* Index in shared objects array */
	size_t sym_idx; /* Index in SO symbol array */
	uint64_t func_addr; /* Raw offset to symbol */
};

struct sym_info {
	uint64_t addr; /* Raw offset to symbol */
	uint64_t size; /* Physical size in the SO*/
	const char *name; /* Symbol */
};

struct shared_obj {
	const char *path; /* Full path to object */
	const char *short_name; /* Basename */
	uint64_t base; /* VA */
	uint64_t size; /* Size from stat() function */
	struct vector symbols; /* Symbols from nm */
	uint8_t found;
};

/* Returns demangled symbols for given address */
const char *ujpp_demangle_getinfo(const struct vector *vec_loaded_so,
				  uint64_t addr, struct demangle_info *di);
/* Free all mem used for symbols */
void ujpp_demangle_free_symtab(struct shared_obj *so);
/* Reads symbol table of given shared object */
void ujpp_demangle_load_so(struct vector *loaded_so, const char *path,
			   uint64_t base, enum so_type type);
/* Used for demanling cfuncs via executable's binary */
void ujpp_demangle(const struct parser_state *ps);

int ujpp_demangle_valid(const struct hvmstate *hvmst);

#endif /* !_UJPP_DEMANGLE_H */
