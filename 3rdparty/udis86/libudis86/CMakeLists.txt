# UJIT: CMake file for udis86 to avoid dependencies on autoconf and libtool.
# UJIT: Based on https://github.com/vmt/udis86/pull/99/files by hasherezade.
# UJIT: Copyright (C) 2020 Anton Soldatov.

set(ITAB_H "${CMAKE_CURRENT_LIST_DIR}/itab.h")
set(ITAB_C "${CMAKE_CURRENT_LIST_DIR}/itab.c")

if(NOT EXISTS ${ITAB_H} OR NOT EXISTS ${ITAB_C})
  message(FATAL_ERROR "You must generate files: ${ITAB_H}, ${ITAB_C} before you start.")
endif()

set(SOURCES_LIBUDIS86
  decode.c
  itab.c
  syn.c
  syn-att.c
  syn-intel.c
  udis86.c
)

add_library(libudis86 STATIC ${SOURCES_LIBUDIS86})

# Fix-ups to avoid patching sources while keeping compilers and linkers happy:

target_compile_options(libudis86
 # Better turn it off than mess with various flavors of the option across
 # different compilers. Reviewed, all fallthrough cases are documented:
 PRIVATE "-Wno-implicit-fallthrough"
 # Pretend we have run autoconf for real:
 PRIVATE "-DHAVE_STRING_H")

# Otherwise we have some linking problems:
set_property(TARGET libudis86 PROPERTY POSITION_INDEPENDENT_CODE ON)
