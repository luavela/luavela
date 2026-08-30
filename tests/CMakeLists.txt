# Running various test suites against uJIT.
# Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

set(CMAKE_MODULE_PATH
  ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
)

# NB! Unfortunately, cmake is unable to rebuild custom targets which
# depend on other custom targets, so we use a variable for convenience.
# See https://cmake.org/pipermail/cmake/2010-September/039555.html.
set(UJIT_CORE_TARGETS libujit_static libujit_shared ujit_static ujit_shared)

# Implementation-independent suites for general compliance testing
add_subdirectory(lang)

# Implementation-dependent suites for either targeted testing of features
# provided by the given implementation or cross-validation one implementation
# with tests from another implementation
add_subdirectory(impl)

#
# uJIT benchmarks: preparations (benchmarks per se are run separately)
#

include(MakeSourceList)
make_source_list(SOURCES_BENCHMARK_DATA
  SOURCES
    gen_benchmark_data.sh
    impl/LuaJIT-tests/suite/bench/fasta.lua
)

add_subdirectory(iponweb/perf/capi)

set(LUAJIT_BENCH_REL_PATH "impl/LuaJIT-tests/suite/bench")
add_custom_command(OUTPUT gen_benchmark_data.ok
  COMMENT "Preparing data for uJIT benchmarks"
  BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/${LUAJIT_BENCH_REL_PATH}/FASTA_ujit.txt
  DEPENDS ${UJIT_CORE_TARGETS} ${SOURCES_BENCHMARK_DATA}
  COMMAND
    env
      LUA_IMPL_BIN=$<TARGET_FILE:${UJIT_TEST_CLI_TARGET_NAME}>
      LUAJIT_BENCH_DIR=${CMAKE_CURRENT_SOURCE_DIR}/${LUAJIT_BENCH_REL_PATH}
      OUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/${LUAJIT_BENCH_REL_PATH}
    ${CMAKE_CURRENT_SOURCE_DIR}/gen_benchmark_data.sh
)

#
# Tests for C interpreter
#

add_custom_command(OUTPUT cinterp_testing_suite
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/binary_ops/binary_ops_meta.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/binary_ops/concat_meta.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/calls/call.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/calls/callm.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/calls/callmt.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/calls/callt.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/calls/varg.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/comparison_ops/iseqn.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/comparison_ops/iseqp.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/comparison_ops/iseqs.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/comparison_ops/iseqv.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/comparison_ops/isge.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/comparison_ops/isgt.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/comparison_ops/isle.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/comparison_ops/islt.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/comparison_ops/isnen.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/comparison_ops/isnep.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/comparison_ops/isnes.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/comparison_ops/isnev.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/fast_functions/assert.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/fast_functions/bit.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/fast_functions/bit_errors.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/fast_functions/coroutine_resume.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/fast_functions/coroutine_wrap.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/fast_functions/getmetatable.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/fast_functions/ipairs.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/fast_functions/math_builtins.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/fast_functions/next.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/fast_functions/pairs.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/fast_functions/pcall.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/fast_functions/rawget.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/fast_functions/setmetatable.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/fast_functions/tonumber.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/fast_functions/tostring.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/fast_functions/string.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/fast_functions/string_errors.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/fast_functions/type.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/fast_functions/xpcall.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/function_headers/ifuncf.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/function_headers/ifuncv.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/loop_ops/fori.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/loop_ops/iterc.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/loop_ops/itern.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/misc.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/returns/ret.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/returns/ret0.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/returns/ret1.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/returns/retm.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/table_ops/gget.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/table_ops/gset.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/table_ops/tdup.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/table_ops/tgetb.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/table_ops/tgets.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/table_ops/tgetv.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/table_ops/tnew.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/table_ops/tsetb.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/table_ops/tsetm.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/table_ops/tsets.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/table_ops/tsetv.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/unary_ops/len.lua
  COMMAND $<TARGET_FILE:ujit_static> tests/cinterp/unary_ops/unm.lua
)

add_custom_target(tests_cinterp
  COMMENT "Tests for C interpreter"
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  DEPENDS cinterp_testing_suite
)

add_custom_target(tailcalls_verifier
  COMMENT "C interpreter tail calls verifier"
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  COMMAND tests/cinterp/run_tailcalls_verifier.sh $<TARGET_FILE:ujit_static>
)

#
# Main targets
#

add_custom_target(tests_smoke
  COMMENT "Running smoke test suites"
)

add_dependencies(tests_smoke
  uJIT-tests-smoke
)

add_custom_target(tests
  COMMENT "Running test suites"
)

add_dependencies(tests
  lua-Harness
  Lua-5.1-tests
  LuaJIT-tests
)

if(NOT APPLE)
  add_dependencies(tests tests_cinterp)
endif()

if(UJIT_LUA52COMPAT)
  add_dependencies(tests CERN-MAD-tests)
endif()

if(UJIT_HAS_JIT AND UJIT_HAS_FFI AND UJIT_LUA52COMPAT AND UJIT_ENABLE_COVERAGE
   AND UJIT_ENABLE_PROFILER AND UJIT_ENABLE_MEMPROF)
  add_dependencies(tests
    tools_tests # located in ../tools/ dir
    uJIT-tests-Lua
  )
  if(UJIT_ENABLE_IPROF AND UJIT_ENABLE_CO_TIMEOUT)
    add_dependencies(tests uJIT-tests-C)
  endif()
endif()

if (UJIT_CINTERP AND UJIT_LUA52COMPAT)
  add_dependencies(tests uJIT-tests-C)
  add_dependencies(tests uJIT-tests-Lua)
  if(UJIT_ENABLE_PROFILER AND UJIT_ENABLE_MEMPROF)
    add_dependencies(tests tools_tests)
  endif()
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_BUILD_TYPE STREQUAL "Release" AND IS_ARM64)
  # ASAN breaks tailcall optimization and is disabled under ARM
  add_dependencies(tests tailcalls_verifier)
endif()

add_custom_target(prepare_benchmarks # not included into ALL
  COMMENT "Preparing to run becnhmarks"
  DEPENDS
    build_capi_benchmarks
    gen_benchmark_data.ok
)
