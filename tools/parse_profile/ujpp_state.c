/*
 * This module used for initializing parser_state, parsing console argumetns
 * and memory management.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "ujpp_utils.h"
#include "ujpp_main.h"
#include "ujpp_read.h"
#include "ujpp_parser.h"
#include "ujpp_demangle_lua.h"

const char *help_str = "\
Parse binary stream of events produced by uJIT profiler and output results\n\
\n\
SYNOPSIS\n\
\n\
ujit-parse-profile --profile profile.bin [options]\n\
\n\
Supported options are:\n\
\n\
 --profile   profile.bin  Path to uJIT profile stream [MANDATORY]\n\
 --exec      executable   Path to executable file to read C symbols from\n\
 --callgraph filename     Generate callgraph in cachegrind format and save it \
into file, specified by filename\n\
 --help                   Show this help and exit\n\
\n\
DESCRIPTION\n\
\n\
Current version provides leaf and callgraph profiles with following \
counters:\n\
\n\
  * VM-level counters for VM states\n\
  * Counters for the LFUNC state (per Lua function)\n\
  * Counters for the CFUNC state (per C function)\n\
  * Counters for the FFUNC state (per Lua built-in function)\n\
  * Counters for states C, INTERP, GC, EXIT, RECORD, OPT, ASM (per\
 shared object)\n\
  * Trace-level counters for the TRACE state\n\
  * Call-graph in cachegrind format\n\
\n\
All data is printed to standard error / output.\n\
";

const size_t size_of_table = sizeof(struct output_table);

void state_free_trace(struct trace *t)
{
	free(t->name);
}

static void state_parse_cmd_args(struct parser_state *ps, int argc, char **argv,
				 const char *short_opt,
				 const struct option *long_opt)
{
	int c;

	if (argc == 1) {
		printf("%s\n", help_str);
		exit(0);
	}

	c = getopt_long(argc, argv, short_opt, long_opt, NULL);

	while (c != -1) {
		switch (c) {
		case -1:
		case 0:
			/* No more arguments. */
			break;
		case 'p':
			ps->profile_name = strdup(optarg);
			break;
		case 'c':
			ps->cg_file_name = strdup(optarg);
			break;

		case 'h':
			printf("%s\n", help_str);
			exit(0);
		case 'e':
			ps->exec_file_name = strdup(optarg);
			break;
		default:
			fprintf(stderr, "%s: invalid option -- %c\n", argv[0],
				c);
			fprintf(stderr, "Use %s --help\n", argv[0]);
			exit(1);
		}

		c = getopt_long(argc, argv, short_opt, long_opt, NULL);
	}
}

static void state_init(struct parser_state *ps)
{
	struct reader *r = &ps->reader;

	ujpp_read_init(r, ps->profile_name);
	ujpp_hash_init(&ps->ht);
	ujpp_hash_init(&ps->ht_trace);

	ujpp_vector_init(&ps->vec_lfunc_cache);
	ujpp_vector_init(&ps->vec_cfunc);
	ujpp_vector_init(&ps->vec_lfunc);
	ujpp_vector_init(&ps->vec_ffunc);
	ujpp_vector_init(&ps->vec_trace);
	ujpp_vector_init(&ps->vec_cfunc_cache);
	ujpp_vector_init(&ps->vec_ffunc_cache);
	ujpp_vector_init(&ps->vec_loaded_so);

	for (size_t hvmst = UJ_VMST_HVMST_START; hvmst < UJ_VMST__MAX; hvmst++)
		ujpp_vector_init(&ps->hvmst_infos[hvmst].vec_rip);
}

void ujpp_state_init(struct parser_state *ps, int argc, char **argv,
		     const char *short_opt, const struct option *long_opt)
{
	const void *table_header = &ps->table_lfunc;

	memset(ps, 0, sizeof(*ps));
	ps->internal_id = 100;

	ps->table_lfunc.names[0] = "| NAME";
	ps->table_lfunc.names[1] = " COUNT";
	ps->table_lfunc.names[2] = " PERCENTAGE";
	ps->table_lfunc.names[3] = " ABSOLUTE";

	ps->table_so.names[0] = "| NAME";
	ps->table_so.names[1] = " BASE";
	ps->table_so.names[2] = " LOADED";
	ps->table_so.names[3] = " (UNUSED)";

	memcpy(&ps->table_vmstate, table_header, size_of_table);
	memcpy(&ps->table_cfunc, table_header, size_of_table);
	memcpy(&ps->table_ffunc, table_header, size_of_table);
	memcpy(&ps->table_trace, table_header, size_of_table);

	for (size_t hvmst = UJ_VMST_HVMST_START; hvmst < UJ_VMST__MAX; hvmst++)
		memcpy(&ps->hvmst_infos[hvmst].tbl, table_header,
		       size_of_table);

	state_parse_cmd_args(ps, argc, argv, short_opt, long_opt);
	state_init(ps);
}

static void state_free_lfunc_cache(struct parser_state *ps)
{
	for (size_t i = 0; i < ps->vec_lfunc_cache.size; i++) {
		struct lfunc_cache *lf_c = ps->vec_lfunc_cache.elems[i];

		if (strcmp(lf_c->sym, LUA_MAIN) != 0)
			free((char *)lf_c->sym);

		assert(NULL != lf_c->demangled_sym);

		if (DEMANGLE_LUA_FAILED != lf_c->demangled_sym)
			free((void *)lf_c->demangled_sym);
	}
}

static void state_free_loaded_so(struct parser_state *ps)
{
	for (size_t i = 0; i < ps->vec_loaded_so.size; i++) {
		struct shared_obj *so = ps->vec_loaded_so.elems[i];

		ujpp_demangle_free_symtab(so);
		free((void *)so->path);
	}
}

static void state_free_trace_callback(void *addr)
{
	struct trace *t = addr;

	free(t->name);
}

static void state_free_traces(struct parser_state *ps)
{
	ujpp_hash_free(&ps->ht_trace, state_free_trace_callback);
}

static void state_free_stack_callback(void *stack)
{
	struct stack *s = stack;

	ujpp_vector_free(s->v);
	free(s->v);
}

static void state_free_stacks(struct parser_state *ps)
{
	ujpp_hash_free(&ps->ht, state_free_stack_callback);
}

void ujpp_state_free(struct parser_state *ps)
{
	state_free_lfunc_cache(ps);
	state_free_loaded_so(ps);
	state_free_traces(ps);
	state_free_stacks(ps);

	free(ps->vec_trace.elems);

	ujpp_vector_free(&ps->vec_lfunc_cache);
	ujpp_vector_free(&ps->vec_cfunc);
	ujpp_vector_free(&ps->vec_lfunc);
	ujpp_vector_free(&ps->vec_ffunc);
	ujpp_vector_free(&ps->vec_cfunc_cache);
	ujpp_vector_free(&ps->vec_ffunc_cache);
	ujpp_vector_free(&ps->vec_loaded_so);

	for (size_t hvmst = UJ_VMST_HVMST_START; hvmst < UJ_VMST__MAX; hvmst++)
		ujpp_vector_free(&ps->hvmst_infos[hvmst].vec_rip);

	if (NULL != ps->profile_name)
		free(ps->profile_name);

	if (NULL != ps->cg_file_name)
		free(ps->cg_file_name);

	ujpp_read_terminate(&ps->reader);
}
