# Build uJIT C API benchmarks
# Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

include_directories(${UJIT_SRC_DIR})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb3")

# Perf tests
set(PERF_TESTS_ROOT_DIR  "${CMAKE_CURRENT_BINARY_DIR}/..")
set(PERF_TESTS_BUILD_DIR "${PERF_TESTS_ROOT_DIR}/tests-run")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PERF_TESTS_BUILD_DIR})

set(CAPI_BENCHMARKS)
function(AddBenchmark BENCHMARK_NAME)
    add_executable(${BENCHMARK_NAME} ${BENCHMARK_NAME}.c)

    set_target_properties(${BENCHMARK_NAME} PROPERTIES
        COMPILE_FLAGS "${TARGET_C_FLAGS}"
    )
    target_link_libraries(${BENCHMARK_NAME} libujit_static m dl)

    set(CAPI_BENCHMARKS ${CAPI_BENCHMARKS} ${BENCHMARK_NAME} PARENT_SCOPE)
endfunction(AddBenchmark)

AddBenchmark(test_table_traversal)

#
# Main target for building all C API benchmarks
#

add_custom_target(build_capi_benchmarks
    COMMENT "Building C API benchmarks"
    DEPENDS ${CAPI_BENCHMARKS}
)
