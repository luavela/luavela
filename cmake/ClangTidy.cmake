# Helpers for integrating clang-tidy checks into the build chain.
# Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

#
# Example usage:
# add_clang_tidy_check(check_target
#   FILES
#      main.c
#      tests.c
#   DEPENDS
#      target1
#      target2
# )
#

set(CLANG_TIDY_RUNNER ${PROJECT_SOURCE_DIR}/scripts/run-clang-tidy)

# FILES should be a list of absolute paths or paths relative to PROJECT_SOURCE_DIR
function(add_clang_tidy_check TARGET_NAME)
  set(prefix ARG)
  set(noValues)
  set(singleValues)
  set(multiValues FILES DEPENDENCIES)

  cmake_parse_arguments(${prefix}
                        "${noValues}"
                        "${singleValues}"
                        "${multiValues}"
                        ${ARGN})

  if (NOT DEFINED ARG_FILES)
    message(FATAL_ERROR "add_clang_tidy_check: need to specify FILES")
  endif()

  add_custom_target(${TARGET_NAME}
    COMMENT "Running ${TARGET_NAME}"
    DEPENDS ${ARG_FILES}
    COMMAND ${CLANG_TIDY_RUNNER} --verbose
            --project ${PROJECT_SOURCE_DIR} --build ${PROJECT_BINARY_DIR}
            ${ARG_FILES}
  )

  if(ARG_DEPENDS)
      add_dependencies(${TARGET_NAME} ${ARG_DEPENDS})
    endif()
endfunction()
