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

add_library(libudis86 OBJECT
  ${UDIS86_DIR}/libudis86/decode.c
  ${UDIS86_DIR}/libudis86/itab.c
  ${UDIS86_DIR}/libudis86/syn.c
  ${UDIS86_DIR}/libudis86/syn-att.c
  ${UDIS86_DIR}/libudis86/syn-intel.c
  ${UDIS86_DIR}/libudis86/udis86.c)

# Fix-ups to avoid patching sources while keeping compilers and linkers happy:

target_compile_options(libudis86
 # All fallthrough cases are documented and shouldn't trigger warnings:
 PRIVATE "-Wno-implicit-fallthrough")

# Pretend we have run autoconf for real:
target_compile_definitions(libudis86 PRIVATE HAVE_STRING_H)

# Otherwise we have some linking problems:
set_target_properties(libudis86 PROPERTIES POSITION_INDEPENDENT_CODE ON)

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

  ExternalProject_Add(jemalloc_ext
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

  ExternalProject_Get_Property(jemalloc_ext SOURCE_DIR)
  set(JEMALLOC_OBJ_DIR ${SOURCE_DIR}/src)
  set(JEMALLOC_OBJ
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

  add_library(jemalloc OBJECT IMPORTED GLOBAL)
  set_target_properties(jemalloc PROPERTIES
    IMPORTED_OBJECTS "${JEMALLOC_OBJ}"
  )
endif()

set(THIRD_PARTY_INCLUDE ${THIRD_PARTY_INCLUDE} PARENT_SCOPE)
