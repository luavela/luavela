# This is a part of uJIT's testing suite.
# Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

list(APPEND SUITE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/README
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/all.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/api.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/attrib.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/big.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/calls.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/checktable.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/closure.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/code.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/constructs.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/db.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/errors.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/etc/ltests.c
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/etc/ltests.h
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/events.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/files.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/gc.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/libs/P1/.hgkeep
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/libs/lib1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/libs/lib11.c
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/libs/lib2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/libs/lib21.c
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/libs/makefile
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/literals.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/locals.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/main.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/math.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/nextvar.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/pm.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/sort.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/strings.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/vararg.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/verybig.lua
)
