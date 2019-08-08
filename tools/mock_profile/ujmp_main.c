/*
 * Fake streams generator for uJIT profiler.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <unistd.h>

#include <lua.h>
#include <lauxlib.h>

#include "../../src/profile/uj_profile_so.h"
#include "../../src/profile/uj_profile_impl.h"

#include "ujmp_mock.h"

#define ERROR(msg)                                                   \
	{                                                            \
		fprintf(stderr, "Error in %s: %s\n", __func__, msg); \
		exit(1);                                             \
	}

static int is_arg(char **argv, const char *str)
{
	return strcmp(argv[2] + 2, str) == 0;
}

static void mock_profile_start(struct profiler_state *ps, lua_State *L,
			       const struct profiler_options *opt, int fd)
{
	uj_profile_start_init(ps, L, opt);

	if (uj_profile_so_init(L, ps) != 0)
		ERROR("Failed to init list of shared objects");

	if (uj_profile_stream_start(ps, L, fd) != LUAE_PROFILE_SUCCESS)
		ERROR("Failed to start streaming");
}

static void mock_profile_stop(struct profiler_state *ps)
{
	/* NB! Stream file is closed during ending the stream */
	if (uj_profile_stream_stop(ps) != LUAE_PROFILE_SUCCESS)
		ERROR("Failed to stop streaming");

	uj_profile_so_free(ps);
}

int main(int argc, char **argv)
{
	struct profiler_state _ps = {0};
	struct profiler_state *ps = &_ps;
	struct ujp_buffer *buf = &ps->buf;
	struct profiler_options opt = {.interval = 55,
				       .mode = PROFILE_CALLGRAPH};
	lua_State *L;
	FILE *fp;

	if (argc != 3)
		ERROR("SYNOPSIS: ujit-mock-profile FILE --TYPE");

	fp = fopen(argv[1], "w");
	if (NULL == fp)
		ERROR("Failed to open stream file");

	if (is_arg(argv, "empty")) {
		/* Special case: an empty file, no heavy machinery needed */
		fclose(fp);
		return 0;
	}

	L = luaL_newstate();
	if (NULL == L)
		ERROR("Lua initialization failed");

	mock_profile_start(ps, L, &opt, fileno(fp));

	if (is_arg(argv, "all"))
		ujmp_mock_all_vmstates(buf);
	else if (is_arg(argv, "wrongffunc"))
		ujmp_mock_wrong_ffunc(buf);
	else if (is_arg(argv, "nobottom"))
		ujmp_mock_no_bottom(buf);
	else if (is_arg(argv, "mainlua"))
		ujmp_mock_main_lua(buf);
	else if (is_arg(argv, "markedlfunc"))
		ujmp_mock_marked_lfunc(buf);
	else if (is_arg(argv, "hvmstates"))
		ujmp_mock_hvmstates(buf);
	else if (is_arg(argv, "lfunc_miscached"))
		ujmp_mock_lfunc_miscached(buf);
	else if (is_arg(argv, "lfunc_diffnames"))
		ujmp_mock_lfunc_diffnames(buf);
	else if (is_arg(argv, "lfunc_difflines"))
		ujmp_mock_lfunc_difflines(buf);
	else if (is_arg(argv, "trace_miscached"))
		ujmp_mock_trace_miscached(buf);
	else if (is_arg(argv, "trace_diffnames"))
		ujmp_mock_trace_diffnames(buf);
	else if (is_arg(argv, "trace_difflines"))
		ujmp_mock_trace_difflines(buf);
	else if (is_arg(argv, "duplicates"))
		ujmp_mock_duplicates(buf);
	else if (is_arg(argv, "lua_demangle"))
		ujmp_mock_lua_demangle(buf);
	else if (is_arg(argv, "lua_demangle_bad_file"))
		ujmp_mock_lua_demangle_badfile(buf);
	else if (is_arg(argv, "lua_demangle_wrong_line"))
		ujmp_mock_lua_demangle_wrongline(buf);
	else if (is_arg(argv, "lua_demangle_no_func"))
		ujmp_mock_lua_demangle_nofunc(buf);
	else if (is_arg(argv, "vdso"))
		ujmp_mock_vdso(buf);
	else
		ERROR("Unknown --mock-type");

	mock_profile_stop(ps);
	lua_close(L);
	fclose(fp);

	return 0;
}
