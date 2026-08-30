# GCC toolchain for building uJIT. To enable, run cmake like this:
#
# $ cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/GCC.cmake ...other options...
#
# Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

set(CMAKE_C_COMPILER "gcc-14")
