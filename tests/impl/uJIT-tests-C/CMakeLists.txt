#
# IPONWEB uJIT tests for C modules
#
# Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

set(SUITE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SUITE_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR}/suite_bin)
set(SUITE_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/suite_run)
set(SUITE_OK_FILE ${SUITE_OUT_DIR}/run_tests.ok)

set(SUITE_RUNNER ${SUITE_DIR}/run_tests.sh)
set(SUITE_RUNNER_COMMON ${SUITE_DIR}/../../run_suite_common.sh)
set(SUITE_SOURCES ${SUITE_RUNNER} ${SUITE_RUNNER_COMMON})

include(Sources.cmake)

add_subdirectory(suite)

set_target_properties(${UJIT_C_TEST_TARGETS}
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${SUITE_BIN_DIR}
)

add_custom_target(uJIT-tests-C DEPENDS
  ${UJIT_C_TEST_TARGETS}
  ${SUITE_OK_FILE}
)

set(LUA_IMPL_BIN $<TARGET_FILE:${UJIT_TEST_CLI_TARGET_NAME}>)
if(UJIT_ENABLE_PROFILER)
  set(PROFILE_PARSER $<TARGET_FILE:ujit-parse-profile>)
endif()

add_custom_command(
  OUTPUT ${SUITE_OK_FILE}
  COMMENT "Running CMocka-based uJIT tests"
  DEPENDS ${UJIT_CORE_TARGETS} ${SUITE_SOURCES}
  COMMAND
    env
      LUA_IMPL_BIN=${LUA_IMPL_BIN}
      SUITE_DIR=${SUITE_DIR}
      SUITE_OUT_DIR=${SUITE_OUT_DIR}
      SUITE_BIN_DIR=${SUITE_BIN_DIR}
      PROFILE_PARSER=${PROFILE_PARSER}
    ${SUITE_RUNNER}
)
