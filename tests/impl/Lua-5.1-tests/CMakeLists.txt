#
# Official test suite for PUC-Rio Lua 5.1 interpreter, adopted.
# Source: https://www.lua.org/tests/
#
# Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

set(SUITE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SUITE_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR}/suite_bin)
set(SUITE_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/suite_run)
set(SUITE_RUNNER ${SUITE_DIR}/run_tests.sh)
set(SUITE_RUNNER_COMMON ${SUITE_DIR}/../../run_suite_common.sh)
set(SUITE_OK_FILE ${SUITE_OUT_DIR}/run_tests.ok)
set(UJIT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../src)

set(SUITE_SOURCES ${SUITE_RUNNER} ${SUITE_RUNNER_COMMON})
include(Sources.cmake)

# We need to build some shared libraries for testing as well
set(SUITE_TEST_LIB_NAMES "lib1;lib11;lib2;lib21")
set(SUITE_TEST_LIB_TARGETS "")
set(SUITE_TEST_LIB_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR})

foreach(SUITE_TEST_LIB_NAME ${SUITE_TEST_LIB_NAMES})
  # e.g. target "test_lua_51_lib1", compiled from "lib1.c", produces "lib1.so"
  set(TEST_LIB_PATH ${SUITE_DIR}/suite/libs)
  set(SUITE_TEST_LIB_TARGET "test_lua_51_${SUITE_TEST_LIB_NAME}")
  set(SUITE_TEST_LIB_SOURCES ${TEST_LIB_PATH}/${SUITE_TEST_LIB_NAME}.c)

  # Fixup for libraries consisting of more than one translation unit:
  if(SUITE_TEST_LIB_NAME STREQUAL "lib11")
    list(APPEND SUITE_TEST_LIB_SOURCES ${TEST_LIB_PATH}/lib1.c)
  elseif(SUITE_TEST_LIB_NAME STREQUAL "lib21")
    list(APPEND SUITE_TEST_LIB_SOURCES ${TEST_LIB_PATH}/lib2.c)
  endif()

  add_library(${SUITE_TEST_LIB_TARGET} SHARED ${SUITE_TEST_LIB_SOURCES})
  target_include_directories(${SUITE_TEST_LIB_TARGET} PRIVATE ${UJIT_INCLUDE_DIR})
  target_link_libraries(${SUITE_TEST_LIB_TARGET} PRIVATE libujit_shared)
  set_target_properties(${SUITE_TEST_LIB_TARGET} PROPERTIES
    PREFIX "" # otherwise we'd get "liblib<name>.so"
    OUTPUT_NAME ${SUITE_TEST_LIB_NAME}
    LIBRARY_OUTPUT_DIRECTORY ${SUITE_BIN_DIR}
    C_VISIBILITY_PRESET default # -fvisibility=hidden is global, but we need symbols visible here
  )

  list(APPEND SUITE_TEST_LIB_TARGETS ${SUITE_TEST_LIB_TARGET})
endforeach()

add_custom_target(Lua-5.1-tests DEPENDS
  ${SUITE_TEST_LIB_TARGETS}
  ${SUITE_OK_FILE}
)

set(LUA_IMPL_BIN $<TARGET_FILE:${UJIT_TEST_CLI_TARGET_NAME}>)
set(LUA_IMPL_OPTIONS ${UJIT_TEST_OPTIONS})

add_custom_command(
  OUTPUT ${SUITE_OK_FILE}
  COMMENT "Running Lua-5.1-tests"
  DEPENDS ${UJIT_CORE_TARGETS} ${SUITE_SOURCES}
  COMMAND
    env
      LUA_IMPL_BIN=${LUA_IMPL_BIN}
      LUA_IMPL_OPTIONS=${LUA_IMPL_OPTIONS}
      SUITE_DIR=${SUITE_DIR}
      SUITE_BIN_DIR=${SUITE_BIN_DIR}
      SUITE_OUT_DIR=${SUITE_OUT_DIR}
      TEST_LIB_NAMES="${SUITE_TEST_LIB_NAMES}"
    ${SUITE_RUNNER}
)
