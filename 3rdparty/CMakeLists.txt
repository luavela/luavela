# Building 3rd-party components for uJIT.
# Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

cmake_minimum_required (VERSION 3.3.2 FATAL_ERROR)

include(ExternalProject)

set(THIRD_PARTY_INCLUDE)

# --- Gather 3rdparty objects for libujit* -------------------------------------

#
# libudis86
#

set(UDIS86_DIR "${CMAKE_CURRENT_SOURCE_DIR}/udis86")

list(APPEND THIRD_PARTY_INCLUDE ${UDIS86_DIR})

set(UDIS86_SOURCES
  ${UDIS86_DIR}/libudis86/decode.c
  ${UDIS86_DIR}/libudis86/itab.c
  ${UDIS86_DIR}/libudis86/syn.c
  ${UDIS86_DIR}/libudis86/syn-att.c
  ${UDIS86_DIR}/libudis86/syn-intel.c
  ${UDIS86_DIR}/libudis86/udis86.c)

# Fix-ups to avoid patching sources while keeping compilers happy:

# All fallthrough cases are documented and shouldn't trigger warnings:
set_source_files_properties(${UDIS86_DIR}/libudis86/decode.c
  PROPERTIES COMPILE_FLAGS -Wno-implicit-fallthrough)

# Pretend we have run autoconf for real:
set_source_files_properties(${UDIS86_DIR}/libudis86/udis86.c
  PROPERTIES COMPILE_DEFINITIONS HAVE_STRING_H)

add_library(libudis86_static OBJECT ${UDIS86_SOURCES})
add_library(libudis86_shared OBJECT ${UDIS86_SOURCES})

set_target_properties(libudis86_shared PROPERTIES POSITION_INDEPENDENT_CODE ON)

#
# jemalloc
#

if(UJIT_ALLOCATOR STREQUAL "jemalloc")
  set(JEMALLOC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/jemalloc-5.0.1")

  if(NOT "${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}") # out-of-source build
    set(JEMALLOC_URL ${JEMALLOC_DIR})
  else()
    set(JEMALLOC_SOURCE_DIR ${JEMALLOC_DIR}) # this will make ExternalProject build in-source
  endif()

  ExternalProject_Add(jemalloc
    PREFIX jemalloc-5.0.1
    URL ${JEMALLOC_URL} # jemalloc source directory, gets copied to build dir in out-of-source build
    SOURCE_DIR ${JEMALLOC_SOURCE_DIR}
    BUILD_IN_SOURCE TRUE # in out-of-source build "source" is in binary dir, so it'll be performed there
    CONFIGURE_COMMAND
      COMMAND <SOURCE_DIR>/autogen.sh
      COMMAND <SOURCE_DIR>/configure CC=${CMAKE_C_COMPILER}
              --disable-cxx --without-export --with-jemalloc-prefix=jem_
    INSTALL_COMMAND cmake -E echo "Skipping install step"
  )

  ExternalProject_Get_Property(jemalloc SOURCE_DIR)
  set(JEMALLOC_OBJ_DIR ${SOURCE_DIR}/src)
  set(JEMALLOC_OBJ
    ${JEMALLOC_OBJ_DIR}/jemalloc.o
    ${JEMALLOC_OBJ_DIR}/arena.o
    ${JEMALLOC_OBJ_DIR}/background_thread.o
    ${JEMALLOC_OBJ_DIR}/base.o
    ${JEMALLOC_OBJ_DIR}/bitmap.o
    ${JEMALLOC_OBJ_DIR}/ckh.o
    ${JEMALLOC_OBJ_DIR}/ctl.o
    ${JEMALLOC_OBJ_DIR}/extent.o
    ${JEMALLOC_OBJ_DIR}/extent_dss.o
    ${JEMALLOC_OBJ_DIR}/extent_mmap.o
    ${JEMALLOC_OBJ_DIR}/hash.o
    ${JEMALLOC_OBJ_DIR}/hooks.o
    ${JEMALLOC_OBJ_DIR}/large.o
    ${JEMALLOC_OBJ_DIR}/malloc_io.o
    ${JEMALLOC_OBJ_DIR}/mutex.o
    ${JEMALLOC_OBJ_DIR}/mutex_pool.o
    ${JEMALLOC_OBJ_DIR}/nstime.o
    ${JEMALLOC_OBJ_DIR}/pages.o
    ${JEMALLOC_OBJ_DIR}/prng.o
    ${JEMALLOC_OBJ_DIR}/prof.o
    ${JEMALLOC_OBJ_DIR}/rtree.o
    ${JEMALLOC_OBJ_DIR}/stats.o
    ${JEMALLOC_OBJ_DIR}/spin.o
    ${JEMALLOC_OBJ_DIR}/sz.o
    ${JEMALLOC_OBJ_DIR}/tcache.o
    ${JEMALLOC_OBJ_DIR}/ticker.o
    ${JEMALLOC_OBJ_DIR}/tsd.o
    ${JEMALLOC_OBJ_DIR}/witness.o
  )
  set(JEMALLOC_OBJ_PIC
    ${JEMALLOC_OBJ_DIR}/jemalloc.pic.o
    ${JEMALLOC_OBJ_DIR}/arena.pic.o
    ${JEMALLOC_OBJ_DIR}/background_thread.pic.o
    ${JEMALLOC_OBJ_DIR}/base.pic.o
    ${JEMALLOC_OBJ_DIR}/bitmap.pic.o
    ${JEMALLOC_OBJ_DIR}/ckh.pic.o
    ${JEMALLOC_OBJ_DIR}/ctl.pic.o
    ${JEMALLOC_OBJ_DIR}/extent.pic.o
    ${JEMALLOC_OBJ_DIR}/extent_dss.pic.o
    ${JEMALLOC_OBJ_DIR}/extent_mmap.pic.o
    ${JEMALLOC_OBJ_DIR}/hash.pic.o
    ${JEMALLOC_OBJ_DIR}/hooks.pic.o
    ${JEMALLOC_OBJ_DIR}/large.pic.o
    ${JEMALLOC_OBJ_DIR}/malloc_io.pic.o
    ${JEMALLOC_OBJ_DIR}/mutex.pic.o
    ${JEMALLOC_OBJ_DIR}/mutex_pool.pic.o
    ${JEMALLOC_OBJ_DIR}/nstime.pic.o
    ${JEMALLOC_OBJ_DIR}/pages.pic.o
    ${JEMALLOC_OBJ_DIR}/prng.pic.o
    ${JEMALLOC_OBJ_DIR}/prof.pic.o
    ${JEMALLOC_OBJ_DIR}/rtree.pic.o
    ${JEMALLOC_OBJ_DIR}/stats.pic.o
    ${JEMALLOC_OBJ_DIR}/spin.pic.o
    ${JEMALLOC_OBJ_DIR}/sz.pic.o
    ${JEMALLOC_OBJ_DIR}/tcache.pic.o
    ${JEMALLOC_OBJ_DIR}/ticker.pic.o
    ${JEMALLOC_OBJ_DIR}/tsd.pic.o
    ${JEMALLOC_OBJ_DIR}/witness.pic.o
  )
  list(APPEND THIRD_PARTY_INCLUDE ${SOURCE_DIR}/include)

  add_library(jemalloc_static OBJECT IMPORTED GLOBAL)
  add_dependencies (jemalloc_static jemalloc)
  set_target_properties(jemalloc_static PROPERTIES
    IMPORTED_OBJECTS "${JEMALLOC_OBJ}"
  )

  add_library(jemalloc_shared OBJECT IMPORTED GLOBAL)
  add_dependencies (jemalloc_shared jemalloc)
  set_target_properties(jemalloc_shared PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    IMPORTED_OBJECTS "${JEMALLOC_OBJ_PIC}"
  )
endif()

set(THIRD_PARTY_INCLUDE ${THIRD_PARTY_INCLUDE} PARENT_SCOPE)
