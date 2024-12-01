# MacOS toolchain for building uJIT. To enable, run cmake like this:
#
# $ cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/MacOS.cmake ...other options...
#
# Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

# XXX: Clang 15 changes the behaviour of -Wnull-pointer-arithmetics to control
# -Wgnu-null-pointer-arithmetics either. Since uJIT is built with -Wextra
# (including -Wnull-pointer-arithmetics) and -Werror flags being set in
# CMAKE_C_FLAGS list, compilation of src/utils/lj_alloc.c fails due to NULL
# pointer arithmetics in TOP_FOOT_SIZE constant definition. Furthermore,
# -Wno-gnu flag to silence GNU extension diagnostics for pointer arithmetic is
# allowed since Clang 15 (https://github.com/llvm/llvm-project/issues/54444)
# too. Considering everything above, tweak the CMAKE_C_FLAGS list for Clang
# toolchain only for Clang 15.0.0 and later.
# XXX: Unlike Linux distributions, we only support one version of macOS, so the
# compiler to be used is well-defined.
# Taking into account all of the above, just provide a tiny tweak for
# CMAKE_C_FLAGS, nothing more.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-gnu-null-pointer-arithmetic")
