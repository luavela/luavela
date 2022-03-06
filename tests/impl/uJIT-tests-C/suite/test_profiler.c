/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

/*
 * Test below is about how profiler stream and parses VM state when
 * VM executes Lua function (initially represented as a form of Lua code
 * chunk) which resides in shared between VMs data.
 *
 * In a real-word scenario several VMs (e.g. operating in different threads)
 * can share data to avoid memory overhead due to having copies of potentially
 * shareable due to sameness data.
 * To mitigate this, the following approach is applied:
 * some data to be shared is marked as data root via luaE_setdataroot(L)
 * function for a particular lua_State *L. And this data is sealed then not
 * to be come a subject for garbage collection.
 * Typically, it's created in some 'parent' i.e. long-living thread.
 * Then, all the other entities (e.g. threads) which want to make use of this
 * data created 'their' VMs using luaE_newdataslave function getting access
 * to the data by luaE_getdataroot function.
 *
 * However, this totally legal use model hadn't been compatible (before the fix)
 * with the profiler streaming way for streaming Lua functions.
 * Here is the description:
 * to tackle the problem of profile streams' sizes (which is mostly due to
 * strings originating from function, symbol names etc.), profiler uses so
 * called 'profcount' to keep the count of the profiling iteration.
 * However, it's not its absolute value which is of particular interest but
 * the moment its value is switched to the next one.
 * Specifically, there is a global (per-VM profcount) and 'local' profcount-s
 * for each func prototype. When each new profiling iteration is initiated,
 * global profcount is incremented. Then, at the first occurence of a function
 * (when profiling dump is stored), individual func proto profcount is bumped
 * which means that it's the first time we've come accross this fuction while
 * profiling so its full signature should be streamed whereas only address
 * is streamed on all the following occasions.
 * However, when there is some shared sealed data which can contain Lua code
 * chunks as well, it would be _one_ profcount available for all the threads
 * working with this proto.
 * So consider the following scenario:
 *
 * <thread #1 - profile #1>:
 *	 g_profcount 0 -> 1
 *	 chunk_profcount 0 -> 1 (reported as 'Lua code from __code element')
 * <thread #2 - profile #2>:
 *	 g_profcount 0 -> 1 (other VM - other g_profcount, from scratch)
 *	 chunk_profcount: 1 (already, and not incemented because g_profcount is
 *			     1 as well)
 *
 * As a result, for the latter profile no full signature is streamed thus
 * the resultant stream is inconsistent.
 *
 * The latter'd ended up in segfault in profile stream parsing.
 * The test reproduces all the steps of the use case described above and
 * verifies streamed profile parsing is successful.
 */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "test_common_lua.h"

#define PARSE_PROFILE_CMD_BUFFER_SIZE 512

struct thread_data_pack {
	lua_State *data_state;
	const char *filename_prefix;
	char *real_filename;
};

/* TODO: use a more verbose function from some common location */
static void load_chunk(lua_State *L, const char *chunk, const char *chunk_name)
{
	int load_exit_code =
		luaL_loadbuffer(L, chunk, strlen(chunk), chunk_name);

	if (LUA_ERRSYNTAX == load_exit_code)
		fprintf(stderr, "Syntax error in lua chunk.\n");
	else if (LUA_ERRMEM == load_exit_code)
		fprintf(stderr,
			"Available memory is not enough to load a chunk.\n");

	if (0 != load_exit_code) {
		const char *error_msg = lua_tostring(L, -1);

		fprintf(stderr, "%s\n", error_msg);
	}
	assert_true(0 == load_exit_code);
}

static void *create_regular_state(void *arg)
{
	const char chunk[] =
		"function profile_test(prof_stream_filename)     \n"
		"       local ujit = require('ujit')             \n"
		"       local started, fname_real =              \n"
		"       ujit.profile.start(2, 'leaf',            \n"
		"                          prof_stream_filename) \n"
		"       t.foo()                                  \n"
		"       local counter, err = ujit.profile.stop() \n"
		"       return fname_real                        \n"
		"end                                             \n";
	struct thread_data_pack *tdp = (struct thread_data_pack *)(arg);
	const char *real_filename;
	size_t real_filename_len;

	lua_State *L = luaE_newdataslave((lua_State *)(tdp->data_state));

	assert_non_null(L);
	luaL_openlibs(L);
	luaE_getdataroot(L);

	assert_true(lua_istable(L, 1));

	lua_setglobal(L, "t");

	load_chunk(L, chunk, "profiler_chunk");

	assert_true(0 == lua_pcall(L, 0, 0, 0));

	lua_getfield(L, LUA_GLOBALSINDEX, "profile_test");

	lua_pushstring(L, tdp->filename_prefix);

	assert_true(0 == lua_pcall(L, 1, 1, 0));
	assert_true(lua_isstring(L, 1));

	real_filename = lua_tostring(L, 1);
	assert_non_null(real_filename);
	real_filename_len = strlen(real_filename);
	tdp->real_filename = malloc(real_filename_len + 1);
	strncpy(tdp->real_filename, real_filename, real_filename_len + 1);

	lua_close(L);

	return NULL;
}

static lua_State *create_data_state()
{
	const char chunk[] = "return { foo =                        \n"
			     /*
		 * Motivation: this function should be relatively long-running
		 * so its running duration is enough for sampling profile to
		 * 'catch' it.
		 */
			     "        function()                    \n"
			     "                local counter = 10000 \n"
			     "                local i = 0           \n"
			     "                while (i < counter)   \n"
			     "                do                    \n"
			     "                        i = i + 1     \n"
			     "               end                    \n"
			     "               return 42 * 1984       \n"
			     "        end }                         \n";

	lua_State *L = test_lua_open();

	luaL_openlibs(L);

	load_chunk(L, chunk, "data_state_chunk");
	assert_true(0 == lua_pcall(L, 0, 1, 0));

	assert_stack_size(L, 1);
	assert_true(lua_istable(L, 1));

	/*
	 * NB: together with using a state a data root i.e. having it serving
	 * as a somewhat data container there is a _must_ to seal this data as
	 * otherwise it'd get garbage collected so the whole point of creating a
	 * dataroot would be in vain.
	 */
	luaE_seal(L, 1);
	luaE_setdataroot(L, 1);

	return L;
}

static char *get_profiler_path(void)
{
	const size_t CWD_BUFFER_LENGTH = 512;
	const char *PROFILE_PARSER_REL_PATH = "/tools/ujit-parse-profile";
	const size_t profile_parser_path_len = strlen(PROFILE_PARSER_REL_PATH);
	char *cwd_path, *ujit_tests_dir_path = NULL, *parser_path = NULL;
	size_t ujit_dir_path_len;
	struct stat parser_stat;

	cwd_path = malloc(CWD_BUFFER_LENGTH + 1);
	getcwd(cwd_path, CWD_BUFFER_LENGTH);

	/*
	 * The assumption is the following:
	 * test/tests are run from somewhere inside 'tests' directory.
	 * 'tests' key can be the only anchor to try to determine:
	 * - ujit dir path
	 * - ujit/tools dir path
	 * As 'tests' reside inside 'ujit' dir, '/tests' can not be absolute
	 * path.
	 */

	ujit_tests_dir_path = strstr(cwd_path, "/tests");

	assert_true(NULL != ujit_tests_dir_path);

	ujit_dir_path_len = ujit_tests_dir_path - cwd_path;

	assert_true(ujit_dir_path_len > 0);

	parser_path = malloc(ujit_dir_path_len + profile_parser_path_len + 1);

	strncpy(parser_path, cwd_path, ujit_dir_path_len);
	strncpy(parser_path + ujit_dir_path_len, PROFILE_PARSER_REL_PATH,
		profile_parser_path_len + 1);

	assert_true(-1 != stat(parser_path, &parser_stat));

	free(cwd_path);
	return parser_path;
}

static int run_profile_stream_parse(char *filename)
{
	const char PROFILE_PARSER_PATH_VAR_NAME[] = "PROFILE_PARSER";
	const char parse_profile_cmd_format_str[] =
		"\"%s\" --profile \"%s\" > /dev/null";
	char parse_profile_cmd_buffer[PARSE_PROFILE_CMD_BUFFER_SIZE];
	const char *parser_env_path;
	char *parser_path = NULL;
	int system_cmd_run_result;

	parser_env_path = getenv(PROFILE_PARSER_PATH_VAR_NAME);
	if (NULL == parser_env_path) {
		parser_path = get_profiler_path();

		parser_env_path = parser_path;
	}

	snprintf(parse_profile_cmd_buffer, PARSE_PROFILE_CMD_BUFFER_SIZE,
		 parse_profile_cmd_format_str, parser_env_path, filename);

	system_cmd_run_result = system(parse_profile_cmd_buffer);

	if (NULL != parser_path)
		free(parser_path);

	free(filename);

	return system_cmd_run_result;
}

static void test_profiler_with_data_state(void **state)
{
	UNUSED_STATE(state);

	pthread_t pthread1, pthread2;
	struct thread_data_pack tdp1, tdp2;

	lua_State *data_state = create_data_state();

	tdp1.data_state = data_state;
	tdp2.data_state = data_state;

	tdp1.filename_prefix = "chunk1.profile.bin";
	tdp2.filename_prefix = "chunk2.profile.bin";

	luaE_profinit();

	pthread_create(&pthread1, NULL, &create_regular_state, (void *)&tdp1);
	pthread_join(pthread1, NULL);

	pthread_create(&pthread2, NULL, &create_regular_state, (void *)&tdp2);
	pthread_join(pthread2, NULL);

	luaE_profterm();
	lua_close(data_state);

	assert_true(!run_profile_stream_parse(tdp1.real_filename) &&
		    !run_profile_stream_parse(tdp2.real_filename));
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_profiler_with_data_state)};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
