# Building docs
# Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
find_program(SPHINX_EXECUTABLE NAMES sphinx-build)

if (SPHINX_EXECUTABLE)
  add_custom_target(docs
    COMMENT "Building HTML documentation"
    COMMAND ${SPHINX_EXECUTABLE} -b html "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/html"
  )

  # Additional `make clean` files
  set_property(DIRECTORY PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
    "${CMAKE_CURRENT_BINARY_DIR}/html"
  )
else()
  message(STATUS "Can't find sphinx-build - 'docs' target not added")
endif()
