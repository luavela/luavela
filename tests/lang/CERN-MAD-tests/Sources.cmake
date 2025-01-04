# This is a part of uJIT's testing suite.
# Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

list(APPEND SUITE_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/suite/luacore.lua
  ${CMAKE_CURRENT_SOURCE_DIR}/suite/luagmath.lua
  ${CMAKE_CURRENT_SOURCE_DIR}/suite/luaobject.lua
  ${CMAKE_CURRENT_SOURCE_DIR}/suite/luaunit.lua
  ${CMAKE_CURRENT_SOURCE_DIR}/suite/luaunitext.lua
  ${CMAKE_CURRENT_SOURCE_DIR}/suite/object.lua
)
