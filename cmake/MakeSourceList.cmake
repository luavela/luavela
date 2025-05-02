# Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

# make_source_list provides a convenient way to define a list of sources
# and get a list of absolute paths.
#
# Example usage:
#
#   make_source_list(SOURCES_CORE
#     SOURCES
#       main.c
#       test.c
#       subdir/test2.c
#   )
#
# This will give you the list:
#    "<...>/main.c;<...>/test.c;<...>/subdir/test2.c"
# (where `<...>` is ${CMAKE_CURRENT_SOURCE_DIR}).
#
# Absolute paths in `SOURCES` list don't get ${CMAKE_CURRENT_SOURCE_DIR}
# prepended to them.

function(make_source_list list)
  set(prefix ARG)
  set(noValues)
  set(singleValues)
  set(multiValues SOURCES)

  cmake_parse_arguments(${prefix}
                        "${noValues}"
                        "${singleValues}"
                        "${multiValues}"
                        ${ARGN})

  set(result_list "")

  foreach(fn ${ARG_SOURCES})
    if (IS_ABSOLUTE ${fn})
      list(APPEND result_list "${fn}")
    else()
      list(APPEND result_list "${CMAKE_CURRENT_SOURCE_DIR}/${fn}")
    endif()
  endforeach()

  set(${list} "${result_list}" PARENT_SCOPE)
endfunction()
