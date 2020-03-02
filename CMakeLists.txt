# uJIT -- interpreter and JIT compiler for Lua language.
# This is the main entry point for building, testing and packaging the project.
# If you need more details:
#  * See ChangeLog for the detailed list of changes;
#  * See INSTALL.rst for guidance on configuring, building and installation;
#  * See README.rst otherwise.
# Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

# --- Initial setup ------------------------------------------------------------
cmake_minimum_required(VERSION 3.3.2 FATAL_ERROR)
project(UJIT C)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(UJIT_TARGET_OS "UJ_TARGET_LINUX")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(UJIT_TARGET_OS "UJ_TARGET_MACOS")
else()
  message(FATAL_ERROR "Unsupported target OS ${CMAKE_SYSTEM_NAME}")
endif()

#
# Fine-tuning cmake environment:
#

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(SetVersion)
include(ClangTidy)

#
# Variables to be exported to child scopes:
#

set(UJIT_SRC_DIR         "${PROJECT_SOURCE_DIR}/src")
set(UJIT_BINARY_DIR      "${PROJECT_BINARY_DIR}/src")


# Names of the CLI binaries
set(UJIT_CLI_NAME_STATIC "ujit")
set(UJIT_CLI_NAME_SHARED "ujit_dyn")

# Symlinks will be created for LuaVela aliases
set(UJIT_CLI_NAME_ALIAS_STATIC "luavela")
set(UJIT_CLI_NAME_ALIAS_SHARED "luavela_dyn")

# Names of targets that build CLI binaries
set(UJIT_CLI_TARGET_NAME_STATIC "ujit_static")
set(UJIT_CLI_TARGET_NAME_SHARED "ujit_shared")

# To enable experimental support of jemalloc allocator, set below to
#    jemalloc
set(UJIT_ALLOCATOR "default")

set(UJIT_TEST_LIB_TYPE "static" CACHE STRING "Type of binary to test with. Can be \"static\" or \"shared\".")
set(UJIT_TEST_LIB_TYPE_VALUES "static;shared")
set_property(CACHE UJIT_TEST_LIB_TYPE PROPERTY STRINGS ${UJIT_TEST_LIB_TYPE_VALUES})

message(STATUS "UJIT_TEST_LIB_TYPE='${UJIT_TEST_LIB_TYPE}'")

# Check that UJIT_TEST_LIB_TYPE value is correct
# In CMake 3.5 we'll be able to use IN_LIST here
list(FIND UJIT_TEST_LIB_TYPE_VALUES ${UJIT_TEST_LIB_TYPE} TEST_LIB_TYPE_INDEX)
if(TEST_LIB_TYPE_INDEX EQUAL -1)
  message(FATAL_ERROR "UJIT_TEST_LIB_TYPE must be \"static\" or \"shared\"")
endif()

# Name of the CLI binary to be used in tests:
set(UJIT_TEST_CLI_TARGET_NAME "ujit_${UJIT_TEST_LIB_TYPE}")

# To pass any options to the uJIT interpreter during testing,
# please set UJIT_TEST_OPTIONS to a raw string of options.

#
# --- Compilation flags setup --------------------------------------------------
#

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wall -pedantic")

# Since the assembler part does NOT maintain a frame pointer, it's pointless
# to slow down the C part by not omitting it. Debugging, tracebacks and
# unwinding are not affected -- the assembler part has frame unwind
# information and GCC emits it where needed (x64) or with -g (see CCDEBUG).
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fomit-frame-pointer")

# All symbols are hidden by default, public API is attributed with LUA*_API
# macros in both headers and translation units. See src/luaconf.h for details.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_DEFAULT_SOURCE") # Enable some POSIX and BSD functionality

set(CMAKE_C_FLAGS_DEBUG "-ggdb3")                       # Re-defined to benefit from expanding macros in gdb
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG -ggdb3")        # Re-defined, since default cmake release optimization level is O3

if(CMAKE_INSTALL_PREFIX)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DLUA_ROOT='\"${CMAKE_INSTALL_PREFIX}\"'")

  if(CMAKE_LIBRARY_ARCHITECTURE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DLUA_MULTILIB='\"lib/${CMAKE_LIBRARY_ARCHITECTURE}\"'")
  endif()
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D${UJIT_TARGET_OS}")

if(UJIT_TARGET_OS STREQUAL "UJ_TARGET_LINUX")
  option(UJIT_ENABLE_GDBJIT "Support for DWARF data for assembled traces" ON)
else()
  set(UJIT_ENABLE_GDBJIT OFF)
endif()
if(UJIT_ENABLE_GDBJIT)
  # NB! This macro needs to be defined in the root C flags because if we define
  # it only in TARGET_C_FLAGS, targets buildvm and core will be built with
  # different versions of struct GCtrace.
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DGDBJIT")
endif()

if(UJIT_TARGET_OS STREQUAL "UJ_TARGET_LINUX")
  option(UJIT_ENABLE_VTUNEJIT "Intel VTune support" OFF)
else()
  set(UJIT_ENABLE_VTUNEJIT OFF)
endif()
if(UJIT_ENABLE_VTUNEJIT)
  # Configure things to instrument the code base with Intel VTune JIT API.
  # We rely on hardcoded installation directory (VTune's default), change
  # this behaviour on demand. We include SDK sources directly instead of
  # linking to libjitprofiling.a because of difficulties with linking static
  # libs against static libs. See notes in 3rdparty/CMakeLists.txt and:
  # * http://stackoverflow.com/questions/3821916/how-to-merge-two-ar-static-libraries-into-one
  # * http://stackoverflow.com/questions/2157629/linking-static-libraries-to-other-static-libraries
  set(VTUNE_DIR_PREFIX "/opt/intel/vtune_amplifier")
  set(VTUNE_INSTALL_DIR "${VTUNE_DIR_PREFIX}")
  if(NOT EXISTS ${VTUNE_INSTALL_DIR})
      set(VTUNE_INSTALL_DIR "${VTUNE_DIR_PREFIX}_xe")
  endif()
  if(EXISTS ${VTUNE_INSTALL_DIR})
    # NB! This macro needs to be defined in the root C flags because if we define
    # it only in TARGET_C_FLAGS, targets buildvm and core will be built with
    # different versions of struct GCtrace.
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DVTUNEJIT")
    include_directories(${VTUNE_INSTALL_DIR}/include)
    # List of necessary files according to
    # https://software.intel.com/en-us/node/596668
    set(VTUNE_SDK_SOURCES
      ${VTUNE_INSTALL_DIR}/sdk/src/ittnotify/jitprofiling.c
    )
    # Suppress complaints about the code base we do not control:
    set_source_files_properties(${VTUNE_SDK_SOURCES} PROPERTIES COMPILE_FLAGS -Wno-pedantic)
  else()
    message(FATAL_ERROR "VTune installation dir '${VTUNE_INSTALL_DIR}' not found.")
  endif()
endif()

## Auxilary flags for main targets (libraries, binaries, unit tests)
set(TARGET_C_FLAGS "-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -U_FORTIFY_SOURCE")

option(UJIT_HAS_JIT "JIT support" ON)
if(NOT UJIT_HAS_JIT)
  set(TARGET_C_FLAGS "${TARGET_C_FLAGS} -DUJIT_DISABLE_JIT")
endif()

option(UJIT_HAS_FFI "FFI support" ON)
if(NOT UJIT_HAS_FFI)
  set(TARGET_C_FLAGS "${TARGET_C_FLAGS} -DUJIT_DISABLE_FFI")
endif()

option(UJIT_LUA52COMPAT "Compatibility with Lua 5.2" ON)
if(UJIT_LUA52COMPAT)
  set(TARGET_C_FLAGS "${TARGET_C_FLAGS} -DUJIT_ENABLE_LUA52COMPAT")
endif()

option(UJIT_ENABLE_THREAD_SAFETY "Enables shared internal data thread safety" ON)
if(UJIT_ENABLE_THREAD_SAFETY)
  set(TARGET_C_FLAGS "${TARGET_C_FLAGS} -DUJIT_IS_THREAD_SAFE")
endif()

if(UJIT_TARGET_OS STREQUAL "UJ_TARGET_LINUX")
  option(UJIT_ENABLE_PROFILER "Sampling profiler support" ON)
else()
  set(UJIT_ENABLE_PROFILER OFF)
endif()
if(UJIT_ENABLE_PROFILER)
  set(TARGET_C_FLAGS "${TARGET_C_FLAGS} -DUJIT_PROFILER")
endif()

option(UJIT_ENABLE_IPROF "Instrumenting profiler support" ON)
if(UJIT_ENABLE_IPROF)
  set(TARGET_C_FLAGS "${TARGET_C_FLAGS} -DUJIT_IPROF_ENABLED")
endif()

option(UJIT_ENABLE_COVERAGE "Platform-level coverage support" ON)
if(UJIT_ENABLE_COVERAGE)
  set(TARGET_C_FLAGS "${TARGET_C_FLAGS} -DUJIT_COVERAGE")
endif()

if(UJIT_TARGET_OS STREQUAL "UJ_TARGET_LINUX")
  option(UJIT_ENABLE_CO_TIMEOUT "Coroutine timeout support" ON)
else()
  set(UJIT_ENABLE_CO_TIMEOUT OFF)
endif()
if(UJIT_ENABLE_CO_TIMEOUT)
  set(TARGET_C_FLAGS "${TARGET_C_FLAGS} -DUJIT_CO_TIMEOUT")
endif()

if(UJIT_TARGET_OS STREQUAL "UJ_TARGET_LINUX")
  option(UJIT_ENABLE_MEMPROF "Memory profiler support" ON)
else()
  set(UJIT_ENABLE_MEMPROF OFF)
endif()
if(UJIT_ENABLE_MEMPROF)
  set(TARGET_C_FLAGS "${TARGET_C_FLAGS} -DUJIT_MEMPROF")
endif()

option(UJIT_USE_VALGRIND "Valgrind support" OFF)
if(UJIT_USE_VALGRIND)
  set(TARGET_C_FLAGS "${TARGET_C_FLAGS} -DUJIT_USE_VALGRIND -g")
endif()

option(UJIT_PROTECT_MCODE "Mcode memory pages protection support" ON)
if(NOT UJIT_PROTECT_MCODE)
  set(TARGET_C_FLAGS "${TARGET_C_FLAGS} -DLUAJIT_UNPROTECT_MCODE")
endif()

set(UJIT_TIMER OFF)
if(UJIT_ENABLE_CO_TIMEOUT OR UJIT_ENABLE_PROFILER OR UJIT_ENABLE_MEMPROF)
  set(UJIT_TIMER ON)
  set(TARGET_C_FLAGS "${TARGET_C_FLAGS} -DUJIT_TIMER")
endif()

# Auxilary flags for the VM core. Clang warns us explicitly that these flags
# are unused. Other compilers, however, are silent, so we still set the flags
# in order not to break anything.
set(TARGET_VM_FLAGS ${TARGET_C_FLAGS})
if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
  set(TARGET_VM_FLAGS "")
endif()

# --- 3rd party dependencies ---------------------------------------------------
add_subdirectory(3rdparty)


# --- Documentation ------ -----------------------------------------------------
add_subdirectory(docs)

# --- pkg-config, man page -----------------------------------------------------
add_subdirectory(etc)

# --- Main source tree ---------------------------------------------------------
add_subdirectory(src)

# --- Test suite ---------------------------------------------------------------
add_subdirectory(tests EXCLUDE_FROM_ALL)

# --- Tools --------------------------------------------------------------------
add_subdirectory(tools)

# --- clang-tidy ---------------------------------------------------------------

add_custom_target(clang_tidy COMMENT "Running clang-tidy")
add_dependencies(clang_tidy
  clang_tidy_core
  clang_tidy_tools
)

if(UJIT_HAS_JIT AND UJIT_HAS_FFI AND UJIT_LUA52COMPAT AND UJIT_ENABLE_COVERAGE
   AND UJIT_ENABLE_PROFILER AND UJIT_ENABLE_MEMPROF AND UJIT_ENABLE_IPROF
   AND UJIT_ENABLE_CO_TIMEOUT)
  add_dependencies(clang_tidy clang_tidy_tests)
endif()

# --- checkpatch ---------------------------------------------------------------

add_custom_target(clang_format
  COMMENT "Running style checks"
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  COMMAND perl scripts/run-clang-format --pretend
)

# --- luacheck -----------------------------------------------------------------

set(LFS_BINARY_DIR "${PROJECT_BINARY_DIR}/3rdparty/lua/luafilesystem")

add_library(lfs SHARED
  ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/lua/luafilesystem/src/lfs.c
)
target_link_libraries(lfs PRIVATE libujit_shared)

set_target_properties (lfs PROPERTIES
  PREFIX ""
  COMPILE_FLAGS "${TARGET_C_FLAGS}"
  LIBRARY_OUTPUT_DIRECTORY "${LFS_BINARY_DIR}"
)

set(LUACHECKABLE_PATHS
  tests/impl/uJIT-tests-C/suite/chunks
  tests/impl/uJIT-tests-Lua/suite/chunks
  tests/iponweb/perf
  tools/parse_memprof
)

add_custom_target(luacheck
  COMMENT "Running Lua code checks"
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  COMMAND
      prove -v scripts/run-luacheck ::
        --lua-bin $<TARGET_FILE:ujit_static>
        --lfs-lib "${LFS_BINARY_DIR}/lfs.so"
        ${LUACHECKABLE_PATHS}
)
add_dependencies(luacheck ujit lfs)

# --- flake8 checks for Python code --------------------------------------------

set(PYTHON_SCRIPTS
  tools/ujit-gdb.py
)

add_custom_target(flake8
  COMMENT "Running flake8 checks for Python code"
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  COMMAND python3 -m flake8 ${PYTHON_SCRIPTS}
)

# --- Convenience targets ------------------------------------------------------

# Run full stability test suite (including extra checks like linting style,
# static analysis, etc.).
add_custom_target(tests_full
  COMMENT "Running full stability test suite"
)

add_dependencies(tests_full
  flake8
  clang_format
  luacheck
  clang_tidy
  tests_smoke
  tests
)

# --- Packaging ----------------------------------------------------------------

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPackUjit.cmake)
  include(CPackUjit)
endif()
