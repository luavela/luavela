-- luacheck suppressions for internal uJIT testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local ujit_tests_lua = 'tests/impl/uJIT-tests-Lua/suite/chunks'

files[ujit_tests_lua .. '/compiler-concat/concat.lua'] = {ignore = {"211", "631"}}
files[ujit_tests_lua .. '/compiler-sink/partsinkstore.lua'] = {ignore = {"542"}}
files[ujit_tests_lua .. '/compiler-sink/sink.lua'] = {ignore = {"542"}, globals = {"g"}}
files[ujit_tests_lua .. '/coverage/coverage_gc.lua'] = {ignore = {"111", "113", "532"}}
files[ujit_tests_lua .. '/dumpbc/chunk.lua'] = {ignore = {"111", "113", "211", "311", "431"}}
files[ujit_tests_lua .. '/dumpbc/long-source-lines.lua'] = {ignore = {"631"}}
files[ujit_tests_lua .. '/dumpbc/try-overflow-hint-buffer.lua'] = {ignore = {"631"}}
files[ujit_tests_lua .. '/immutable/immutable-global.lua'] = {globals = {"FOO"}}
files[ujit_tests_lua .. '/leb128/bc-dump.lua'] = {ignore = {"211"}, globals = {"foo"}}
files[ujit_tests_lua .. '/leb128/syntax-error.lua'] = {ignore = {"113"}}
files[ujit_tests_lua .. '/metrics-gc/allocated-freed.lua'] = {ignore = {"231", "311"}}
files[ujit_tests_lua .. '/metrics-strhash/strhash.lua'] = {ignore = {"211"}}
files[ujit_tests_lua .. '/movtv/gset.lua'] = {globals = {"FOO"}}
files[ujit_tests_lua .. '/table/recording.lua'] = {ignore = {"111"}}
files[ujit_tests_lua .. '/table/recording/sink_shallowcopy.lua'] = {ignore = {"231"}}
files[ujit_tests_lua .. '/usesfenv/usesfenv.lua'] = {globals = {"GLOBAL_FLAG2", "GLOBAL_FLAG1"}}

local ujit_tests_c = 'tests/impl/uJIT-tests-C/suite/chunks'

files[ujit_tests_c .. '/bc_hotcnt/all.lua'] = {globals = {"all_cases", "dumped_all_cases", "loaded_all_cases", "update_counters"}}
files[ujit_tests_c .. '/test_ext_events/tracing_during_timeout.lua'] = {globals = {"timeout_handler", "timeout_handler_called", "coroutine_payload"}}
files[ujit_tests_c .. '/test_gc_traverse_stack/gc_after_mm_exit.lua'] = {ignore = {"211"}}
files[ujit_tests_c .. '/test_gc_traverse_stack/gc_before_mm_exit.lua'] = {ignore = {"211"}}
files[ujit_tests_c .. '/test_profiler_and_timeouts/profile_timeouts.lua'] = {globals = {"coroutine_start", "chunk_start", "chunk_exit"}}
files[ujit_tests_c .. '/test_stack_resize/rec_ff.lua'] = {ignore = {"211"}, globals = {"payload"}}

local perf_tests = 'tests/iponweb/perf'

files[perf_tests .. '/capi/chunks/lua_next.lua'] = {globals = {"traverse_next"}}
files[perf_tests .. '/capi/chunks/luae_iterate.lua'] = {globals = {"traverse_iterate"}}
files[perf_tests .. '/chunks/history_strings.lua'] = {ignore = {"631"}}
files[perf_tests .. '/chunks/tf_idf_corpus.lua'] = {ignore = {"631"}}
