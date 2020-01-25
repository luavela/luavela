# Test suites borrowed from other Lua implementations
# Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

add_subdirectory(Lua-5.1-tests)
add_subdirectory(LuaJIT-tests)
add_subdirectory(uJIT-tests-smoke)

if (UJIT_HAS_JIT AND UJIT_HAS_FFI AND UJIT_LUA52COMPAT)
  add_subdirectory(uJIT-tests-C)
  add_subdirectory(uJIT-tests-Lua)
endif()
