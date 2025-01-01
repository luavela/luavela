#
# François Perrad's lua-Harness suite
# Source: https://fperrad.frama.io/lua-Harness
#
# Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

set(SUITE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SUITE_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/suite_run)
set(SUITE_RUNNER ${SUITE_DIR}/run_tests.sh)
set(SUITE_RUNNER_COMMON ${SUITE_DIR}/../../run_suite_common.sh)
set(SUITE_OK_FILE ${SUITE_OUT_DIR}/run_tests.ok)

set(SUITE_SOURCES ${SUITE_RUNNER} ${SUITE_RUNNER_COMMON})
include(Sources.cmake)

add_custom_target(lua-Harness DEPENDS ${SUITE_OK_FILE})

set(LUA_IMPL_BIN $<TARGET_FILE:${UJIT_TEST_CLI_TARGET_NAME}>)
set(LUA_IMPL_OPTIONS ${UJIT_TEST_OPTIONS})

if(UJIT_LUA52COMPAT)
  set(LUA_TEST_PROFILE "profile_luajit20_compat52")
else()
  set(LUA_TEST_PROFILE "profile_luajit20")
endif()

add_custom_command(
  OUTPUT ${SUITE_OK_FILE}
  COMMENT "Running lua-Harness"
  DEPENDS ${UJIT_CORE_TARGETS} ${SUITE_SOURCES}
  COMMAND
    env
      LUA_IMPL_BIN=${LUA_IMPL_BIN}
      LUA_IMPL_OPTIONS=${LUA_IMPL_OPTIONS}
      SUITE_DIR=${SUITE_DIR}
      SUITE_OUT_DIR=${SUITE_OUT_DIR}
      LUA_TEST_PROFILE=${LUA_TEST_PROFILE}
    ${SUITE_RUNNER}
)
