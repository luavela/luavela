/*
 * This module implements cachegrind call-graph generator.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "ujpp_main.h"
#include "ujpp_utils.h"
#include "ujpp_demangle_lua.h"
#include "../../src/profile/ujp_write.h"

/* If some symbol was flushed we need it's local cachegrind id */
static uint64_t callgraph_get_flush_id(const struct parser_state *ps,
				       const struct frame *f)
{
	const struct cfunc_cache *cache;

	switch (f->type) {
	case UJP_FRAME_TYPE_CFUNC:
		cache = ps->vec_cfunc_cache.elems[f->cache_id];
		return cache->flushed;
	case UJP_FRAME_TYPE_LFUNC:
	case UJP_FRAME_TYPE_MAIN:
		cache = ps->vec_lfunc_cache.elems[f->cache_id];
		return cache->flushed;
	case UJP_FRAME_TYPE_FFUNC:
		cache = ps->vec_ffunc_cache.elems[f->cache_id];
		return cache->flushed;
	default:
		ujpp_utils_die("wrong frame type %lu", f->type);
		break;
	}

	return 0;
}

static void callgraph_write_cfunc(struct parser_state *ps,
				  struct cfunc_cache *cache, FILE *fp)
{
	if (cache->flushed)
		return;
	if (NULL == cache->symbol)
		fprintf(fp, "fn=(%" PRIu64 ") 0x%" PRIx64 "\n", ps->internal_id,
			cache->addr);
	else
		fprintf(fp, "fn=(%" PRIu64 ") %s\n", ps->internal_id,
			cache->symbol);
	cache->flushed = ps->internal_id++;
}

static void callgraph_write_lfunc(struct parser_state *ps,
				  struct lfunc_cache *cache, FILE *fp)
{
	if (cache->flushed)
		return;
	if (DEMANGLE_LUA_FAILED != cache->demangled_sym)
		fprintf(fp, "fn=(%" PRIu64 ") %s (%s:%" PRIu64 ")\n",
			ps->internal_id, cache->demangled_sym, cache->sym,
			cache->line);
	else
		fprintf(fp, "fn=(%" PRIu64 ") %s:%" PRIu64 "\n",
			ps->internal_id, cache->sym, cache->line);
	cache->flushed = ps->internal_id++;
}

static void callgraph_write_ffunc(struct parser_state *ps,
				  struct ffunc_cache *cache, FILE *fp)
{
	if (cache->flushed)
		return;
	fprintf(fp, "fn=(%" PRIu64 ") %s\n", ps->internal_id,
		ujpp_utils_ffunc_name(cache->ffid));
	cache->flushed = ps->internal_id++;
}

static void callgraph_write_frame_info(struct parser_state *ps,
				       const struct frame *f, FILE *fp)
{
	const size_t id = f->cache_id;

	switch (f->type) {
	case UJP_FRAME_TYPE_CFUNC:
		callgraph_write_cfunc(ps, ps->vec_cfunc_cache.elems[id], fp);
		break;
	case UJP_FRAME_TYPE_LFUNC:
	case UJP_FRAME_TYPE_MAIN:
		callgraph_write_lfunc(ps, ps->vec_lfunc_cache.elems[id], fp);
		break;
	case UJP_FRAME_TYPE_FFUNC:
		callgraph_write_ffunc(ps, ps->vec_ffunc_cache.elems[id], fp);
		break;
	default:
		ujpp_utils_die("wrong frame type %lu", f->type);
	}
}

/* Write stack sample to file */
static void callgraph_write_stack(struct parser_state *ps, FILE *fp,
				  const struct stack *s)
{
	const struct frame *last;
	uint64_t last_id;

	for (size_t i = 0; i < s->v->size; i++)
		callgraph_write_frame_info(ps, s->v->elems[i], fp);

	fprintf(fp, "\n");

	for (size_t i = s->v->size - 1; i > 0; i--) {
		const struct frame *f = s->v->elems[i];
		const struct frame *f1 = s->v->elems[i - 1];
		uint64_t func_id = callgraph_get_flush_id(ps, f);
		uint64_t cfn_id = callgraph_get_flush_id(ps, f1);

		fprintf(fp,
			"fn=(%" PRIu64 ")\ncfn=(%" PRIu64 ")\ncalls=%" PRIu64
			" 0\n0 1\n",
			func_id, cfn_id, s->calls);
	}

	last = s->v->elems[s->v->size - 1];
	last_id = callgraph_get_flush_id(ps, last);

	fprintf(fp,
		"fn=(1) main\ncfn=(%" PRIu64 ")\ncalls=%" PRIu64 " 0\n0 1\n\n",
		last_id, s->calls);
}

void ujpp_callgraph_generate(struct parser_state *ps)
{
	FILE *fp;

	if (NULL == ps->cg_file_name)
		return;

	fp = fopen(ps->cg_file_name, "w");

	if (NULL == fp) {
		ujpp_utils_die("can't open callgraph file - %s",
			       ps->cg_file_name);
		return; /* unreachable */
	}

	fprintf(fp, "events: Ir\n");

	for (size_t i = 0; i < HASH_TABLE_SIZE; i++) {
		if (NULL == ps->ht.arr[i])
			continue;
		for (size_t j = 0; j < ps->ht.arr[i]->size; j++)
			callgraph_write_stack(ps, fp, ps->ht.arr[i]->elems[j]);
	}

	fclose(fp);
}
