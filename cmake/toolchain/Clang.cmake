# Clang toolchain for building uJIT. To enable, run cmake like this:
#
# $ cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/Clang.cmake ...other options...
#
# Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

set(CMAKE_C_COMPILER "clang-6.0")
