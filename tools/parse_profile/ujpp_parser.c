/*
 * This module is used to read streamed samples.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "ujpp_main.h"
#include "ujpp_utils.h"
#include "ujpp_read.h"
#include "../../src/uj_ff.h"
#include "../../src/profile/ujp_write.h"

const char *LUA_MAIN = "LUA_MAIN";

enum read_frame_mode {
	READ_FRAME_TOPMOST, /* Read the top-most frame of the Lua stack */
	READ_FRAME_LOWER /*
			  * Read lower frames of the Lua stack
			  * (from topmost - 1 to the lowest one)
			  */
};

typedef void (*read_stack_handler)(struct parser_state *ps, uint8_t vmstate);

/*
 * Adds one VM state for which only shared object info is reported
 * with checking that this shared object isn't already added.
 */
static void parser_add_hvmstate(struct hvmstate_info *hvmsti,
				struct vector *vec_loaded_so, uint64_t addr)
{
	struct demangle_info info = {0};
	const char *name = ujpp_demangle_getinfo(vec_loaded_so, addr, &info);
	struct hvmstate *hvmst;

	for (size_t i = 0; i < hvmsti->vec_rip.size; i++) {
		hvmst = hvmsti->vec_rip.elems[i];

		if (info.so_idx != -1 && info.func_addr != -1) {
			if (hvmst->di.so_idx == info.so_idx &&
			    hvmst->di.func_addr == info.func_addr) {
				hvmst->count++;
				return;
			}
		} else {
			if (hvmst->addr == addr) {
				hvmst->count++;
				return;
			}
		}
	}

	hvmst = ujpp_utils_allocz(sizeof(*hvmst));
	hvmst->addr = addr;
	hvmst->count = 1;
	hvmst->symbol = name;
	memcpy(&hvmst->di, &info, sizeof(struct demangle_info));
	ujpp_vector_add(&hvmsti->vec_rip, hvmst);
}

/* Adds one C function to cfunc's cache chain. */
static uint64_t parser_cache_cfunc(struct parser_state *ps, uint64_t addr)
{
	struct cfunc_cache *cf_cache;

	for (size_t i = 0; i < ps->vec_cfunc_cache.size; i++) {
		cf_cache = ps->vec_cfunc_cache.elems[i];

		if (cf_cache->addr == addr)
			return i;
	}

	cf_cache = ujpp_utils_allocz(sizeof(*cf_cache));
	cf_cache->addr = addr;

	ujpp_vector_add(&ps->vec_cfunc_cache, cf_cache);
	return ps->vec_cfunc_cache.size - 1;
}

/*
 * Adds one C function to the top frames chain. id is an index from cfunc's
 * cache chain.
 */
static void parser_add_cfunc(struct parser_state *ps, uint64_t id)
{
	struct cfunc *cf;

	for (size_t i = 0; i < ps->vec_cfunc.size; i++) {
		cf = ps->vec_cfunc.elems[i];

		if (cf->cache_id == id) {
			cf->count++;
			return;
		}
	}

	cf = ujpp_utils_allocz(sizeof(*cf));
	cf->count = 1;
	cf->cache_id = id;

	ujpp_vector_add(&ps->vec_cfunc, cf);
}

/*
 * When Lua function is streamed two or more times with UJP_EXPLICIT_SYMBOL,
 * each entry must have the same name and code line.
 */
static void parser_check_lfunc_collision(const struct lfunc_cache *lfc,
					 const char *name, const uint64_t line)
{
	if (strcmp(lfc->sym, name) != 0 || lfc->line != line)
		ujpp_utils_die("Lua symbol collision: %s was first declared"
			       " on line %d, but now line %d is reported",
			       lfc->sym, lfc->line, line);
}

/* Do the same as cache_cfunc */
static uint64_t parser_cache_lfunc(struct parser_state *ps, uint64_t addr,
				   const char *name, uint64_t line, int *found)
{
	struct lfunc_cache *lfc;

	for (size_t i = 0; i < ps->vec_lfunc_cache.size; i++) {
		lfc = ps->vec_lfunc_cache.elems[i];

		if (lfc->addr != addr)
			continue;

		if (name)
			parser_check_lfunc_collision(lfc, name, line);

		if (found != NULL)
			*found = 1;
		return i;
	}

	/*
	 * Missing cache symbol means that there is no entry with
	 * UJP_EXPLICIT_SYMBOL found in stream.
	 */
	if (!name)
		ujpp_utils_die("Can't find cached symbol for marked lfunc",
			       NULL);

	lfc = ujpp_utils_allocz(sizeof(*lfc));
	lfc->sym = (char *)name;
	lfc->line = line;
	lfc->addr = addr;

	ujpp_vector_add(&ps->vec_lfunc_cache, lfc);

	return ps->vec_lfunc_cache.size - 1;
}

/* The same as add_cfunc, but for lfunc's */
static void parser_add_lfunc(struct parser_state *ps, uint64_t id)
{
	struct lfunc *lf;

	for (size_t i = 0; i < ps->vec_lfunc.size; i++) {
		lf = ps->vec_lfunc.elems[i];

		if (lf->cache_id == id) {
			lf->count++;
			return;
		}
	}

	lf = ujpp_utils_allocz(sizeof(*lf));
	lf->count = 1;
	lf->cache_id = id;

	ujpp_vector_add(&ps->vec_lfunc, lf);
}

/* The same as cache_cfunc, but for ffunc's */
static uint64_t parser_cache_ffunc(struct parser_state *ps, uint64_t ffid)
{
	struct ffunc_cache *ff_cache;

	for (size_t i = 0; i < ps->vec_ffunc_cache.size; i++) {
		ff_cache = ps->vec_ffunc_cache.elems[i];

		if (ff_cache->ffid == ffid)
			return i;
	}

	ff_cache = ujpp_utils_allocz(sizeof(*ff_cache));
	ff_cache->ffid = ffid;

	ujpp_vector_add(&ps->vec_ffunc_cache, ff_cache);

	return ps->vec_ffunc_cache.size - 1;
}

static void parser_add_ffunc(struct parser_state *ps, uint64_t id)
{
	struct ffunc *ff;

	for (size_t i = 0; i < ps->vec_ffunc.size; i++) {
		ff = ps->vec_ffunc.elems[i];

		if (ff->cache_id == id) {
			ff->count++;
			return;
		}
	}

	ff = ujpp_utils_allocz(sizeof(*ff));
	ff->count = 1;
	ff->cache_id = id;

	ujpp_vector_add(&ps->vec_ffunc, ff);
}

/* Add trace to the top frames chain */
static void parser_add_trace(struct parser_state *ps, uint64_t traceno,
			     char *name, uint64_t line, uint64_t generation)
{
	struct trace *t = ujpp_utils_allocz(sizeof(*t));
	void *cached;
	unsigned int hash;

	t->generation = generation;
	t->traceno = traceno;
	t->count = 1;
	t->name = name;
	t->line = line;

	hash = ujpp_hash_avalanche(traceno * generation);
	cached = ujpp_hash_insert(&ps->ht_trace, hash, t, ujpp_utils_cmp_trace);

	if (!cached) {
		/*
		 * Marked trace without previosly streamed explicit
		 * symbol is impossible.
		 */
		if (!t->name && !line)
			ujpp_utils_die("Can't find cached symbol for trace",
				       NULL);
		return;
	}

	/*
	 * This trace was already streamed with UJP_EXPLICIT_SYMBOL flag,
	 * so name must be freed since it's already cached. Checking for NULL
	 * is just a paranoia.
	 */
	if (t->name)
		free(t->name);
	free(t);
	t = cached;
	t->count++;
}

static struct frame_header parser_read_frame_header(struct parser_state *ps)
{
	struct frame_header fr = {0};
	struct reader *r = &ps->reader;

	fr.frame_header = ujpp_read_u8(r);
	fr.frame_type = (fr.frame_header >> 3) & 0x07;
	fr.frame_id = ujpp_read_u64(r);
	return fr;
}

static void parser_add_stack(struct parser_state *ps, struct stack *s)
{
	void *hashed =
		ujpp_hash_insert(&ps->ht, s->hash, s, ujpp_utils_cmp_stack);

	if (!hashed)
		return;

	ujpp_vector_free(s->v);
	free(s->v);
	free(s);
	s = hashed;
	s->calls++;
}

static int parser_read_frame(struct parser_state *ps, struct stack *s,
			     enum read_frame_mode mode)
{
	struct frame_header fr;
	struct frame *frame;
	uint64_t id = 0;
	struct reader *r = &ps->reader;

	fr = parser_read_frame_header(ps);

	/* Top frame already parsed, but there is another top frame? */
	if (mode == READ_FRAME_LOWER && fr.frame_header & 1)
		ujpp_utils_die("top frame collision", NULL);

	frame = ujpp_utils_allocz(sizeof(*frame));
	frame->type = fr.frame_type;

	if (fr.frame_type == UJP_FRAME_TYPE_CFUNC) {
		id = parser_cache_cfunc(ps, fr.frame_id);

		if (mode == READ_FRAME_TOPMOST)
			parser_add_cfunc(ps, id);
	} else if (fr.frame_type == UJP_FRAME_TYPE_LFUNC) {
		char *name = NULL;
		uint64_t loc = 0;
		int found = 0;

		if (fr.frame_header & UJP_FRAME_EXPLICIT_SYMBOL) {
			name = ujpp_read_str(r);
			loc = ujpp_read_u64(r);
		}

		id = parser_cache_lfunc(ps, fr.frame_id, name, loc, &found);

		/*
		 * Work-around for special case, when some Lua function
		 * streamed twice and more with UJP_EXPLICIT_SYMBOL flag.
		 * To avoid duplication, no new entry will be added
		 * to cache and we must free memory allocated for symbol.
		 */
		if (found && name != NULL)
			free(name);

		if (mode == READ_FRAME_TOPMOST)
			parser_add_lfunc(ps, id);
	} else if (fr.frame_type == UJP_FRAME_TYPE_FFUNC) {
		id = parser_cache_ffunc(ps, fr.frame_id);

		if (mode == READ_FRAME_TOPMOST)
			parser_add_ffunc(ps, id);
	} else if (fr.frame_type == UJP_FRAME_TYPE_MAIN) {
		id = parser_cache_lfunc(ps, fr.frame_id, LUA_MAIN, 0, NULL);

		if (mode == READ_FRAME_TOPMOST)
			parser_add_lfunc(ps, id);
	} else {
		ujpp_utils_die("wrong frame type in stack", NULL);
	}

	frame->cache_id = id;

	if (fr.frame_header & UJP_FRAME_BOTTOM) {
		free(frame);
		return 1;
	} else {
		ujpp_vector_add(s->v, frame);
		s->hash += ujpp_hash_avalanche(id + frame->type);
	}

	return 0;
}

static void parser_unwind_stack(struct parser_state *ps, uint8_t vmstate)
{
	UNUSED(vmstate);

	struct stack *s = ujpp_utils_allocz((sizeof(*s)));
	int read_stack_mode = READ_FRAME_TOPMOST;

	s->v = ujpp_utils_allocz((sizeof(*s->v)));

	if (NULL == s->v) {
		free(s); /* unreachable */
		return; /* unreachable */
	}

	s->calls = 1;
	s->hash = 0;
	ujpp_vector_init(s->v);

	while (parser_read_frame(ps, s, read_stack_mode) == 0)
		if (read_stack_mode == READ_FRAME_TOPMOST)
			read_stack_mode = READ_FRAME_LOWER;

	parser_add_stack(ps, s);
}

static void parser_read_frame_trace(struct parser_state *ps, uint8_t vmstate)
{
	UNUSED(vmstate);
	struct reader *r = &ps->reader;
	struct frame_header fr;
	char *name = NULL;
	uint64_t line = 0;
	uint64_t generation;

	fr = parser_read_frame_header(ps);
	generation = ujpp_read_u64(r);

	if (fr.frame_type != UJP_FRAME_TYPE_TRACE)
		ujpp_utils_die("wrong frame type", NULL);

	if (fr.frame_header & UJP_FRAME_EXPLICIT_SYMBOL) {
		name = ujpp_read_str(r);
		line = ujpp_read_u64(r);
	}

	parser_add_trace(ps, fr.frame_id, name, line, generation);
}

static void parser_read_frame_hvmstate(struct parser_state *ps,
				       enum vmstate vmstate)
{
	struct frame_header fr = parser_read_frame_header(ps);

	if (fr.frame_type != UJP_FRAME_TYPE_HOST)
		ujpp_utils_die("wrong frame type", NULL);

	if (fr.frame_header & UJP_FRAME_EXPLICIT_SYMBOL)
		ujpp_utils_die("explicit symbol", NULL);

	parser_add_hvmstate(&ps->hvmst_infos[vmstate], &ps->vec_loaded_so,
			    fr.frame_id);
}

static void parser_read_frame_ffunc(struct parser_state *ps, uint8_t vmstate)
{
	UNUSED(vmstate);

	struct frame_header fr = parser_read_frame_header(ps);
	uint64_t id;

	if (fr.frame_type != UJP_FRAME_TYPE_FFUNC)
		ujpp_utils_die("wrong frame type", NULL);

	if (fr.frame_header & UJP_FRAME_EXPLICIT_SYMBOL)
		ujpp_utils_die("explicit symbol", NULL);

	id = parser_cache_ffunc(ps, fr.frame_id);

	if (fr.frame_id > FF__MAX)
		ujpp_utils_die("bad ffunc ffid: %d, max ffid: %d", fr.frame_id,
			       FF__MAX);

	parser_add_ffunc(ps, id);
}

static const read_stack_handler rs_handlers[] = {
	(read_stack_handler)parser_unwind_stack, /* UJ_VMST_LFUNC */
	(read_stack_handler)parser_read_frame_ffunc, /* UJ_VMST_FFUNC */
	(read_stack_handler)parser_unwind_stack, /* UJ_VMST_CFUNC */
	(read_stack_handler)parser_read_frame_hvmstate, /* UJ_VMST_IDLE */
	(read_stack_handler)parser_read_frame_hvmstate, /* UJ_VMST_INTERP */
	(read_stack_handler)parser_read_frame_hvmstate, /* UJ_VMST_GC */
	(read_stack_handler)parser_read_frame_hvmstate, /* UJ_VMST_EXIT */
	(read_stack_handler)parser_read_frame_hvmstate, /* UJ_VMST_RECORD */
	(read_stack_handler)parser_read_frame_hvmstate, /* UJ_VMST_OPT */
	(read_stack_handler)parser_read_frame_hvmstate, /* UJ_VMST_ASM */
	(read_stack_handler)parser_read_frame_trace /* UJ_VMST_TRACE */
};

void ujpp_parser_read_stack(struct parser_state *ps, uint8_t vmstate)
{
	if (vmstate > UJ_VMST_TRACE) {
		ujpp_utils_die("not supported vmstate", NULL);
		return;
	}

	read_stack_handler rs_handler = rs_handlers[vmstate];

	if (NULL == rs_handler) {
		ujpp_utils_die("read stack handler for vmstate is NULL", NULL);
		return;
	}

	rs_handler(ps, vmstate);
}
