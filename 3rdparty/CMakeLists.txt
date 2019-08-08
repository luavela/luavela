# Building 3rd-party components for uJIT.
# Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

cmake_minimum_required (VERSION 3.3.2 FATAL_ERROR)

include(ExternalProject)

set(THIRD_PARTY_INCLUDE)

# --- Gather 3rdparty objects for libujit* -------------------------------------
# 1. When you link a static library to another static library, they do not get
#    "merged", you still have two libraries, and it is your final executable
#    which has to link with both.
# 2. To work this around, let us not *link* libujit* to its dependencies.
#    Instead, let's take their object files (we have them, right?), append
#    them to the list of object files of libujit* and assemble final libraries
#    from all these object files.

#
# libudis86
#

set(UDIS86_DIR "${CMAKE_CURRENT_SOURCE_DIR}/udis86")

if(NOT "${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}") # out-of-source build
  set(UDIS86_URL ${UDIS86_DIR})
else()
  set(UDIS86_SOURCE_DIR ${UDIS86_DIR}) # this will make ExternalProject build in-source
endif()

ExternalProject_Add(udis86_ext
  PREFIX udis86
  URL ${UDIS86_URL} # udis86 source directory, gets copied to binary dir in out-of-source build
  SOURCE_DIR ${UDIS86_SOURCE_DIR}
  BUILD_IN_SOURCE TRUE # in out-of-source build "source" is in binary dir, so it'll be performed there
  CONFIGURE_COMMAND
    COMMAND <SOURCE_DIR>/autogen.sh
    COMMAND <SOURCE_DIR>/configure CC=${CMAKE_C_COMPILER}
            --without-yasm --without-sphinx-build
  INSTALL_COMMAND cmake -E echo "Skipping install step"
)

ExternalProject_Get_Property(udis86_ext SOURCE_DIR)

# NB! Enlist files explicitly (no globbing) so that cmake generates a valid
# file list for the linker before udis86 is built
set(UDIS86_OBJ_DIR ${SOURCE_DIR}/libudis86/.libs)
set(UDIS86_OBJ
  ${UDIS86_OBJ_DIR}/decode.o
  ${UDIS86_OBJ_DIR}/itab.o
  ${UDIS86_OBJ_DIR}/udis86.o
  ${UDIS86_OBJ_DIR}/syn.o
  ${UDIS86_OBJ_DIR}/syn-att.o
  ${UDIS86_OBJ_DIR}/syn-intel.o
)
list(APPEND THIRD_PARTY_INCLUDE ${SOURCE_DIR})

add_library(udis86 OBJECT IMPORTED GLOBAL)
set_target_properties(udis86 PROPERTIES
  IMPORTED_OBJECTS "${UDIS86_OBJ}"
)

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

#
# LuaFileSystem
#

add_library(lfs SHARED
  ${CMAKE_CURRENT_SOURCE_DIR}/lua/luafilesystem/src/lfs.c
)

set_target_properties (lfs PROPERTIES
  PREFIX ""
  COMPILE_FLAGS "${TARGET_C_FLAGS}"
  LIBRARY_OUTPUT_DIRECTORY "lua/luafilesystem"
)
