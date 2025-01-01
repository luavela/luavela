# Running various test suites against uJIT.
# Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

cmake_minimum_required (VERSION 3.3.2 FATAL_ERROR)

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

add_custom_target(prepare_benchmarks # not included into ALL
  COMMENT "Preparing to run becnhmarks"
  DEPENDS
    build_capi_benchmarks
    gen_benchmark_data.ok
)
