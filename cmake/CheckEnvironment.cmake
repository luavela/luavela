# Checks that the build environment meets the minimum compiler requirements.
# Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(gcc_version "14.2.0")
    if(CMAKE_C_COMPILER_VERSION VERSION_LESS gcc_version)
        message(FATAL_ERROR "GCC version must be at least ${gcc_version}!")
    endif()
elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set(clang_version "20.1.2")
    if(CMAKE_C_COMPILER_VERSION VERSION_LESS clang_version)
        message(FATAL_ERROR "Clang version must be at least ${clang_version}!")
    endif()
elseif(CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
    set(clang_version "15.0.0")
    if(CMAKE_C_COMPILER_VERSION VERSION_LESS clang_version)
        message(FATAL_ERROR "AppleClang version must be at least ${clang_version}!")
    endif()
else()
    message(FATAL_ERROR "Couldn't use unsupported compiler - ${CMAKE_C_COMPILER_ID}")
endif()
