# Building supplementary materials for uJIT.
# Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

cmake_minimum_required (VERSION 3.3.2 FATAL_ERROR)

set(UJIT_MAN_PAGE ${CMAKE_CURRENT_SOURCE_DIR}/ujit.1)
set(UJIT_COMPRESSED_MAN_PAGE ${CMAKE_CURRENT_BINARY_DIR}/ujit.1.gz)

add_custom_command(
  COMMENT "Compressing uJIT manual page"
  OUTPUT ${UJIT_COMPRESSED_MAN_PAGE}
  DEPENDS ${UJIT_MAN_PAGE}
  COMMAND ${CMAKE_COMMAND} -E tar "zcf" ${UJIT_COMPRESSED_MAN_PAGE} -- ${UJIT_MAN_PAGE}
)

configure_file(
  ujit.pc.in
  ujit.pc
  @ONLY ESCAPE_QUOTES
)

add_custom_target(etc ALL
  COMMENT "Building supplementary materials"
  DEPENDS
    ${UJIT_COMPRESSED_MAN_PAGE}
)
