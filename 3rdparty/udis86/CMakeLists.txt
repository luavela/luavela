# UJIT: CMake file for udis86 to avoid dependencies on autoconf and libtool.
# UJIT: Based on https://github.com/vmt/udis86/pull/99/files by hasherezade.
# UJIT: Copyright (C) 2020 Anton Soldatov.
# UJIT: Only the library is built, CLI is irrelevant for this project.

cmake_minimum_required(VERSION 3.3.2 FATAL_ERROR)

project(udis86 C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -Werror -Wall -Wextra -pedantic")

add_subdirectory(libudis86)
