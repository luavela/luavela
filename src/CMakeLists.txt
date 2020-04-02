# Building uJIT core: bootstrapping, VM, runtime, JIT compiler.
# Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

cmake_minimum_required (VERSION 3.3.2 FATAL_ERROR)
enable_language (ASM)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}) # for sub-projects
include_directories(${THIRD_PARTY_INCLUDE})

include(MakeSourceList)

# --- Define source tree -------------------------------------------------------

make_source_list(SOURCES_RUNTIME # Core runtime
  SOURCES
    lj_bc.c
    lj_debug.c
    uj_dispatch.c
    luajit.c
    uj_hook.c
    uj_err.c
    uj_errmsg.c
    uj_dwarf.c
    uj_throw.c
    uj_unwind.c
    uj_func.c
    uj_proto.c
    uj_upval.c
    lj_gc.c
    uj_mem.c
    uj_lib.c
    uj_meta.c
    uj_mtab.c
    uj_obj.c
    uj_obj_seal.c
    uj_obj_immutable.c
    uj_state.c
    uj_str.c
    uj_cstr.c
    uj_sbuf.c
    lj_tab.c
    uj_udata.c
    uj_vmstate.c
    uj_sigtimer.c
    uj_timerint.c
    uj_strhash.c
    uj_capi.c
    uj_capi_aux.c
    uj_capi_ext.c
    uj_hotcnt.c
    uj_coverage.c
    lib/init.c
)

if(UJIT_ENABLE_GDBJIT)
  list(APPEND SOURCES_RUNTIME uj_gdbjit.c)
endif()

if(UJIT_ENABLE_VTUNEJIT)
  list(APPEND SOURCES_RUNTIME uj_vtunejit.c)
endif()

make_source_list(SOURCES_FRONTEND # Lua frontend
  SOURCES
    frontend/lj_lex.c
    frontend/lj_parse.c
    frontend/lj_bcread.c
    frontend/lj_bcwrite.c
)

make_source_list(SOURCES_LUA_LIB # Lua standard library (+ extensions by LuaJIT and uJIT)
  # NB! Please do not changes the order of the libraries (required by buildvm).
  SOURCES
    lib/base.c lib/math.c lib/bit.c lib/string.c
    lib/table.c lib/io.c lib/os.c lib/package.c
    lib/debug.c lib/jit.c lib/ffi.c
    lib/ujit.c
)

make_source_list(SOURCES_JIT_CORE # JIT compiler, core part
  SOURCES
    jit/lj_trace.c
    jit/lj_record.c
    jit/uj_record_indexed.c
    jit/lj_ffrecord.c
    jit/lj_ir.c
    jit/lj_asm.c
    jit/lj_mcode.c
    jit/lj_snap.c
)

make_source_list(SOURCES_JIT_EMITTER # JIT compiler, emitter of machine code:
  SOURCES
    jit/emit/lj_emit.c
    jit/emit/uj_emit_ct.c
    jit/emit/uj_emit_sse2.c
)

make_source_list(SOURCES_JIT_OPT # JIT compiler, machine-independent optimisations:
  SOURCES
    jit/opt/dce.c
    jit/opt/fold.c
    jit/opt/loop.c
    jit/opt/mem.c
    jit/opt/movtv.c
    jit/opt/narrow.c
    jit/opt/sink.c
)

make_source_list(SOURCES_JIT_OPT_ARCH # JIT compiler, machine-dependent optimisations:
  SOURCES
    jit/opt_x86/fold_ins.c
)

set(SOURCES_DUMPER_REL
    dump/uj_dump_bc.c
    dump/uj_dump_stack.c
    dump/uj_dump_utils.c
    dump/uj_dump_datadef.c
)

if(UJIT_HAS_JIT)
  list(APPEND SOURCES_DUMPER_REL
    dump/uj_dump_progress.c
    dump/uj_dump_ir.c
    dump/uj_dump_mcode.c
  )
endif()

make_source_list(SOURCES_DUMPER
  SOURCES
    ${SOURCES_DUMPER_REL}
)

set(SOURCES_PROFILER_REL
    # Sampling performance profiler
    profile/uj_profile.c

    # Memory profiler
    profile/uj_memprof.c

    # Instrumenting profiler
    profile/uj_iprof.c

    # Aux
    profile/uj_symtab.c
    profile/ujp_write.c
)

if(UJIT_ENABLE_PROFILER)
  list(APPEND SOURCES_PROFILER_REL
    profile/uj_profile_stream.c
    profile/uj_profile_so.c
  )
endif()

make_source_list(SOURCES_PROFILER
  SOURCES
    ${SOURCES_PROFILER_REL}
)

make_source_list(SOURCES_UTILS
  SOURCES
    utils/cpuinfo.c
    utils/fp.c
    utils/leb128.c
    utils/lj_char.c
    utils/uj_math.c
    utils/uj_vmmath.c
    utils/uj_crc.c
    utils/random.c
    utils/str.c
    utils/strscan.c
    utils/x86_inslen.c
    utils/strhash/city.c
    utils/strhash/luajit2.c
    utils/strhash/murmur3.c
)

set_source_files_properties(utils/strhash/city.c PROPERTIES COMPILE_FLAGS "-O3 -msse4.2 -std=c99")

if (UJIT_ALLOCATOR STREQUAL "jemalloc")
  set (SOURCES_UTILS_ALLOCATOR utils/jemalloc.c)
else ()
  set (SOURCES_UTILS_ALLOCATOR utils/lj_alloc.c)
endif ()

make_source_list(SOURCES_UTILS
  SOURCES
    ${SOURCES_UTILS}
    ${SOURCES_UTILS_ALLOCATOR}
)

make_source_list(SOURCES_FFI
  SOURCES
    ffi/lj_carith.c
    ffi/lj_ccallback.c
    ffi/lj_ccall.c
    ffi/lj_cconv.c
    ffi/lj_cdata.c
    ffi/lj_clib.c
    ffi/lj_cparse.c
    ffi/lj_crecord.c
    ffi/lj_ctype.c
)

make_source_list(SOURCES_JIT
  SOURCES
    ${SOURCES_JIT_CORE}
    ${SOURCES_JIT_EMITTER}
    ${SOURCES_JIT_OPT}
    ${SOURCES_JIT_OPT_ARCH}
)

make_source_list(SOURCES_CORE_NO_JIT_FFI # Everything except FFI and JIT:
  SOURCES
    ${SOURCES_RUNTIME}
    ${SOURCES_LUA_LIB}
    ${SOURCES_FRONTEND}
    ${SOURCES_DUMPER}
    ${SOURCES_PROFILER}
    ${SOURCES_UTILS}
)

set(SOURCES_CORE
  ${SOURCES_CORE_NO_JIT_FFI}
  # misc:
  ${VTUNE_SDK_SOURCES}
)

if(UJIT_HAS_JIT)
  list(APPEND SOURCES_CORE ${SOURCES_JIT})
endif()

if(UJIT_HAS_FFI)
  list(APPEND SOURCES_CORE ${SOURCES_FFI})
  if(NOT UJIT_HAS_JIT)
    # needed for lj_mcode_sync
    list(APPEND SOURCES_CORE ${CMAKE_CURRENT_SOURCE_DIR}/jit/lj_mcode.c)
  endif()
endif()

# --- Build minilua ------------------------------------------------------------
add_executable (minilua host/minilua.c)
target_link_libraries (minilua m)
set_target_properties (minilua PROPERTIES RUNTIME_OUTPUT_DIRECTORY "host")

# --- Prepare dasm-ed VM description -------------------------------------------
set(MINILUA_FLAGS "")
if (UJIT_HAS_JIT)
  list(APPEND MINILUA_FLAGS -D JIT)
endif()

if (UJIT_HAS_FFI)
  list(APPEND MINILUA_FLAGS -D FFI)
endif()

set (DYNASM_PATH "${PROJECT_SOURCE_DIR}/dynasm")
add_custom_command (
  COMMENT "Generating buildvm_arch.h"
  OUTPUT host/buildvm_arch.h
  COMMAND host/minilua ${DYNASM_PATH}/dynasm.lua ${MINILUA_FLAGS} -o host/buildvm_arch.h ${CMAKE_CURRENT_SOURCE_DIR}/vm_x86.dasc
  DEPENDS minilua vm_x86.dasc ${DYNASM_PATH}/dynasm.lua
)

set (BUILDVM "host/buildvm")

# --- Build buildvm ------------------------------------------------------------
add_executable (buildvm
  host/buildvm.c
  host/buildvm_asm.c
  host/buildvm_lib.c
  host/buildvm_fold.c
  host/buildvm_arch.h
)
set_target_properties (buildvm PROPERTIES RUNTIME_OUTPUT_DIRECTORY "host")
# NB! buildvm includes core headers and thus has to be built with the same
# flags and defines as the uJIT core itself.
set_target_properties (buildvm PROPERTIES COMPILE_FLAGS "${TARGET_C_FLAGS}")
target_include_directories(buildvm PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/host
)

# --- Prepare files generated by buildvm ---------------------------------------
if(UJIT_TARGET_OS STREQUAL "UJ_TARGET_LINUX")
  set(BUILDVM_MODE elfasm)
elseif(UJIT_TARGET_OS STREQUAL "UJ_TARGET_MACOS")
  set(BUILDVM_MODE machasm)
endif()
## VM assembly
add_custom_command (
  OUTPUT lj_vm.s
  COMMAND $<TARGET_FILE:buildvm> -m ${BUILDVM_MODE} -o lj_vm.s
  DEPENDS buildvm
)

## Bytecode definitions
add_custom_command (
  OUTPUT lj_bcdef.h
  COMMAND $<TARGET_FILE:buildvm> -m bcdef -o lj_bcdef.h ${SOURCES_LUA_LIB}
  DEPENDS buildvm ${SOURCES_LUA_LIB}
)

## Fast function definitions
add_custom_command (
  OUTPUT lj_ffdef.h
  COMMAND $<TARGET_FILE:buildvm> -m ffdef -o lj_ffdef.h ${SOURCES_LUA_LIB}
  DEPENDS buildvm ${SOURCES_LUA_LIB}
)

## Library definitions
add_custom_command (
  OUTPUT lj_libdef.h
  COMMAND $<TARGET_FILE:buildvm> -m libdef -o lj_libdef.h ${SOURCES_LUA_LIB}
  DEPENDS buildvm ${SOURCES_LUA_LIB}
)

## Recorder definitions
add_custom_command (
  OUTPUT lj_recdef.h
  COMMAND $<TARGET_FILE:buildvm> -m recdef -o lj_recdef.h ${SOURCES_LUA_LIB}
  DEPENDS buildvm ${SOURCES_LUA_LIB}
)

## Fold definitions
add_custom_command (
  OUTPUT lj_folddef.h
  COMMAND $<TARGET_FILE:buildvm> -m folddef -o lj_folddef.h ${CMAKE_CURRENT_SOURCE_DIR}/jit/opt/fold.c
  DEPENDS buildvm jit/opt/fold.c
)

add_custom_target (
  buildvm_output
  DEPENDS lj_bcdef.h lj_ffdef.h lj_libdef.h lj_recdef.h lj_folddef.h
)

# --- Generate core and VM object files ---------------------------------------

## Virtual machine
add_library (vm_static OBJECT lj_vm.s)
add_library (vm_shared OBJECT lj_vm.s)

set_property (TARGET vm_shared APPEND PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property (TARGET vm_static vm_shared APPEND PROPERTY COMPILE_FLAGS "${TARGET_VM_FLAGS}")

## Platform core
add_library (core_static OBJECT ${SOURCES_CORE})
add_library (core_shared OBJECT ${SOURCES_CORE})

set_property (TARGET core_shared APPEND PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property (TARGET core_static core_shared APPEND PROPERTY COMPILE_FLAGS "${TARGET_C_FLAGS}")

target_include_directories(core_static PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(core_shared PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

add_dependencies (core_static buildvm_output libudis86_static)
add_dependencies (core_shared buildvm_output libudis86_shared)
if(UJIT_ALLOCATOR STREQUAL "jemalloc")
  add_dependencies (core_static jemalloc_static)
  add_dependencies (core_shared jemalloc_shared)
endif()

# --- Generate output ----------------------------------------------------------

##
## Compiling library binaries (static, shared)
##

set (LIB_NAME ujit)

set (LIB_OBJECTS_STATIC
  $<TARGET_OBJECTS:vm_static>
  $<TARGET_OBJECTS:core_static>
  $<TARGET_OBJECTS:libudis86_static>
)

set (LIB_OBJECTS_SHARED
  $<TARGET_OBJECTS:vm_shared>
  $<TARGET_OBJECTS:core_shared>
  $<TARGET_OBJECTS:libudis86_shared>
)

if(UJIT_ALLOCATOR STREQUAL "jemalloc")
  list(APPEND LIB_OBJECTS_STATIC $<TARGET_OBJECTS:jemalloc_static>)
  list(APPEND LIB_OBJECTS_SHARED $<TARGET_OBJECTS:jemalloc_shared>)
endif()

add_library (libujit_static STATIC ${LIB_OBJECTS_STATIC})
set_target_properties (libujit_static PROPERTIES
  OUTPUT_NAME ${LIB_NAME}
  COMPILE_FLAGS "${TARGET_C_FLAGS}"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

set(LIBUJIT_SHARED_LINK_FLAGS "")
if(NOT UJIT_TARGET_OS STREQUAL "UJ_TARGET_MACOS")
  # macOS linker does not support version scripts.
  # Otherwise assume they are supported.
  set(LIBUJIT_SHARED_LINK_FLAGS "${LIBUJIT_SHARED_LINK_FLAGS} -Wl,--version-script,${UJIT_SRC_DIR}/libujit.map")
endif()
add_library (libujit_shared SHARED ${LIB_OBJECTS_SHARED})
set_target_properties (libujit_shared PROPERTIES
  OUTPUT_NAME ${LIB_NAME}
  COMPILE_FLAGS "${TARGET_C_FLAGS}"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  VERSION ${UJIT_VERSION_STRING}
  SOVERSION ${UJIT_VERSION_MAJOR}
  LINK_FLAGS "${LIBUJIT_SHARED_LINK_FLAGS}"
)

##
## Linking libraries
##

target_link_libraries (libujit_static m)
# NB! For the shared lib, please retain dependency on libdl for correct linking.
target_link_libraries (libujit_shared m dl)

if (UJIT_ENABLE_THREAD_SAFETY)
  target_link_libraries (libujit_static pthread)
  target_link_libraries (libujit_shared pthread)
endif ()

if (UJIT_TIMER)
  target_link_libraries (libujit_static rt)
  target_link_libraries (libujit_shared rt)
endif ()

##
## Compiling CLIs
##

make_source_list(CLI_SOURCES
  SOURCES
    ujit.c
    cli/opt.c
)

add_executable (ujit_static ${CLI_SOURCES})
set_target_properties (ujit_static PROPERTIES
  COMPILE_FLAGS "${TARGET_C_FLAGS}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  OUTPUT_NAME ${UJIT_CLI_NAME_STATIC}
)
target_include_directories(ujit_static PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
)

add_executable (ujit_shared ${CLI_SOURCES})
set_target_properties (ujit_shared PROPERTIES
  COMPILE_FLAGS "${TARGET_C_FLAGS}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  OUTPUT_NAME ${UJIT_CLI_NAME_SHARED}
)
target_include_directories(ujit_shared PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
)

##
## Linking CLIs
##

target_link_libraries (ujit_static libujit_static m dl)
target_link_libraries (ujit_shared libujit_shared m dl)

##
## Convenience aliases
##

add_custom_target (libujit  ALL DEPENDS libujit_static libujit_shared)
add_custom_target (ujit_cli ALL DEPENDS    ujit_static    ujit_shared)

# Create alias LuaVela symlinks
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${UJIT_CLI_NAME_ALIAS_STATIC}"
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${UJIT_CLI_NAME_STATIC} ${UJIT_CLI_NAME_ALIAS_STATIC}
  DEPENDS ujit_static
)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${UJIT_CLI_NAME_ALIAS_SHARED}"
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${UJIT_CLI_NAME_SHARED} ${UJIT_CLI_NAME_ALIAS_SHARED}
  DEPENDS ujit_shared
)

add_custom_target (create_luavela_symlinks ALL
  DEPENDS
    "${CMAKE_CURRENT_BINARY_DIR}/${UJIT_CLI_NAME_ALIAS_STATIC}"
    "${CMAKE_CURRENT_BINARY_DIR}/${UJIT_CLI_NAME_ALIAS_SHARED}"
)

add_custom_target (ujit ALL
  DEPENDS
    libujit
    ujit_cli
    create_luavela_symlinks
)

# Create installable targets for subsequent packaging
install (TARGETS ujit_static
  DESTINATION bin
  RUNTIME
  COMPONENT ujit
)
install (TARGETS libujit_static
  DESTINATION lib
  ARCHIVE
  COMPONENT ujit
)
install (TARGETS libujit_shared
  DESTINATION lib
  LIBRARY
  COMPONENT ujit
)
install (FILES
  "${CMAKE_CURRENT_BINARY_DIR}/${UJIT_CLI_NAME_ALIAS_STATIC}" # Symlink to ujit
  DESTINATION bin
  COMPONENT ujit
)

install (FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ujit.h"      # Main header for uJIT
    luaconf.h   # Configuration header
    lua.h       # C API for Lua
    lualib.h    # Lua standard libraries
    lauxlib.h   # Auxiliary library's C API
    lextlib.h   # uJIT-specific extensions of the C API
    lua.hpp     # Convenience wrapper for C++
  DESTINATION include/ujit
    PERMISSIONS
      OWNER_READ OWNER_WRITE
      GROUP_READ
      WORLD_READ
  COMPONENT ujit
)

set(UJIT_ETC_DIR ${PROJECT_BINARY_DIR}/etc)

install (FILES
    ${UJIT_ETC_DIR}/ujit.1.gz
  DESTINATION share/man/man1
    PERMISSIONS
      OWNER_READ OWNER_WRITE
      GROUP_READ
      WORLD_READ
  COMPONENT ujit)
install (FILES
    ${UJIT_ETC_DIR}/ujit.pc
  DESTINATION lib/pkgconfig
    PERMISSIONS
      OWNER_READ OWNER_WRITE
      GROUP_READ
      WORLD_READ
  COMPONENT ujit)

# --- clang-tidy ---------------------------------------------------------------

add_clang_tidy_check (clang_tidy_core
  FILES
    ${SOURCES_JIT}
    ${SOURCES_CORE_NO_JIT_FFI}
  DEPENDS
    buildvm_output
)
