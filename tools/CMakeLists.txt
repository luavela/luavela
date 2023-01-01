# Building tools for developing with uJIT.
# Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

cmake_minimum_required (VERSION 3.3.2 FATAL_ERROR)
enable_language(C)

include(MakeSourceList)

include_directories(
    ${UJIT_SRC_DIR}
    ${UJIT_BINARY_DIR}
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set(UJIT_TOOL_TARGETS)

if(UJIT_ENABLE_PROFILER)
  make_source_list(SOURCES_UJPP
    SOURCES
      parse_profile/ujpp_main.c
      parse_profile/ujpp_table.c
      parse_profile/ujpp_vector.c
      parse_profile/ujpp_utils.c
      parse_profile/ujpp_parser.c
      parse_profile/ujpp_callgraph.c
      parse_profile/ujpp_demangle.c
      parse_profile/ujpp_output.c
      parse_profile/ujpp_hash.c
      parse_profile/ujpp_read.c
      parse_profile/ujpp_state.c
      parse_profile/ujpp_demangle_lua.c
      parse_profile/ujpp_elf.c
  )

  make_source_list(SOURCES_UJMP
    SOURCES
      mock_profile/ujmp_main.c
      mock_profile/ujmp_mock.c
  )

  add_executable(ujit-parse-profile
      ${SOURCES_UJPP}
      ../src/uj_vmstate.c
      ../src/utils/leb128.c)

  add_executable(ujit-mock-profile
      ${SOURCES_UJMP}
      ../src/utils/leb128.c)

  target_link_libraries(ujit-mock-profile
    ${UJIT_BINARY_DIR}/libujit.a -lpthread -lrt -ldl -lm
  )

  target_link_libraries(ujit-parse-profile -lstdc++)

  set_target_properties (ujit-parse-profile PROPERTIES
      COMPILE_FLAGS "${TARGET_C_FLAGS}"
  )

  set_target_properties (ujit-mock-profile PROPERTIES
      COMPILE_FLAGS "${TARGET_C_FLAGS}"
  )

  add_dependencies(ujit-parse-profile buildvm_output)
  add_dependencies(ujit-mock-profile ujit)

  list(APPEND UJIT_TOOL_TARGETS
    ujit-parse-profile
    ujit-mock-profile
  )

  # Create installable targets for subsequent packaging
  install(TARGETS ujit-parse-profile
    RUNTIME
    DESTINATION bin
    COMPONENT tools)
  if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ujit-profile")
    install(FILES
      ${CMAKE_CURRENT_SOURCE_DIR}/ujit-profile
    DESTINATION bin
    PERMISSIONS
      OWNER_READ OWNER_WRITE OWNER_EXECUTE
      GROUP_READ GROUP_EXECUTE
      WORLD_READ WORLD_EXECUTE
    COMPONENT tools)
  endif()

endif()

if(UJIT_ENABLE_MEMPROF)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ujit-memprof")
    install(FILES
      ${CMAKE_CURRENT_SOURCE_DIR}/ujit-memprof
    DESTINATION bin
    PERMISSIONS
      OWNER_READ OWNER_WRITE OWNER_EXECUTE
      GROUP_READ GROUP_EXECUTE
      WORLD_READ WORLD_EXECUTE
    COMPONENT tools)
  endif()
  install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/ujit-parse-memprof
  DESTINATION bin
  PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
  COMPONENT tools)
  install(FILES
      ${CMAKE_CURRENT_SOURCE_DIR}/parse_memprof/bufread.lua
      ${CMAKE_CURRENT_SOURCE_DIR}/parse_memprof/main.lua
      ${CMAKE_CURRENT_SOURCE_DIR}/parse_memprof/parse_memprof.lua
      ${CMAKE_CURRENT_SOURCE_DIR}/parse_memprof/parse_symtab.lua
      ${CMAKE_CURRENT_SOURCE_DIR}/parse_memprof/view_plain.lua
   DESTINATION share/ujit/parse_memprof
   PERMISSIONS
     OWNER_READ OWNER_WRITE
     GROUP_READ
     WORLD_READ
   COMPONENT tools)
endif()

install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/ujit-analyze-dumps
DESTINATION bin
PERMISSIONS
  OWNER_READ OWNER_WRITE OWNER_EXECUTE
  GROUP_READ GROUP_EXECUTE
  WORLD_READ WORLD_EXECUTE
COMPONENT tools)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/ujit-gdb.py
  # > script-name is formed by ensuring that the file name of objfile is
  # > absolute, following all symlinks, and resolving . and .. components, and
  # > appending the -gdb.ext suffix. If this file exists and is readable, GDB
  # > will evaluate it as a script in the specified extension language.
  # considering the most common configuration gdb will search in
  # /usr/share/gdb/auto-load/<absolute-path-to-ujit>-gdb.py
  # For more info go to the following link:
  # https://sourceware.org/gdb/current/onlinedocs/gdb/objfile_002dgdbdotext-file.html#objfile_002dgdbdotext-file
  DESTINATION share/gdb/auto-load/${CMAKE_INSTALL_PREFIX}/bin
  PERMISSIONS
    OWNER_READ OWNER_WRITE
    GROUP_READ
    WORLD_READ
  COMPONENT tools)

if(UJIT_ENABLE_PROFILER AND UJIT_ENABLE_MEMPROF)
  make_source_list(SOURCES_TOOLS_TESTS
    SOURCES
      tests/chunks/demangle/funcdefs.lua
      tests/chunks/demangle/morefuncs.lua
      tests/chunks/payload.lua
      tests/chunks/xpcall.lua
      tests/memprof_parser.t
      tests/profiler_parser.t
      tests/profiler_parser_counters.t
      tests/profiler_parser_demangling.t
      tests/run_iponweb_tools_tests.sh
  )

  add_custom_command(OUTPUT tests/run_iponweb_tools_tests.ok
    COMMENT "Running uJIT tests for tools"
    DEPENDS ${UJIT_TOOL_TARGETS} ${SOURCES_TOOLS_TESTS}
    COMMAND
      env
        TOOLS_BIN_DIR=${CMAKE_CURRENT_BINARY_DIR}
        UJIT_BIN_DIR=$<TARGET_FILE_DIR:${UJIT_TEST_CLI_TARGET_NAME}>
        TEST_LIB_DIR=${UJIT_SRC_DIR}/../tests/impl/uJIT-tests-Lua/suite/lib
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_iponweb_tools_tests.sh
  )

  add_custom_target(tools_tests
    COMMENT "Running tools test suites"
    DEPENDS
     tests/run_iponweb_tools_tests.ok
  )
endif()

make_source_list(SOURCES_TOOLS # The entire code base of tools
  SOURCES
    ${SOURCES_UJPP}
    ${SOURCES_UJMP}
)

add_custom_target(tools ALL
    COMMENT "Building uJIT tools"
    DEPENDS ${UJIT_TOOL_TARGETS}
)

#
# clang-tidy
#

add_clang_tidy_check(clang_tidy_tools
  FILES
    ${SOURCES_TOOLS}
  DEPENDS
    buildvm_output
)
