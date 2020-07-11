Installation
============

Prerequisites
-------------

As of current implementation, LuaVela can be built and run on various x64 Linux
distros and macOS 10.13+. Please note, that 32-bit environment is not supported
and it will not be built correctly.

The build is performed by CMake of version 3.3.2 or higher and was tested with
following compilers:

  * GCC 4.6.3 (Ubuntu 14.04)
  * GCC 4.8.4 (Ubuntu 14.04)
  * GCC 7.3.0 (Ubuntu 18.04)
  * GCC 7.4.0 (Ubuntu 18.04)
  * GCC 8.3.0 (Ubuntu 18.04)
  * GCC 8.4.0 (Ubuntu 18.04)
  * GCC 8.4.0 (Ubuntu 20.04)
  * Clang 3.8 (Ubuntu 14.04)
  * Clang 3.9 (Ubuntu 14.04)
  * Clang 6.0 (Ubuntu 18.04)
  * AppleClang 10.0.0 (macOS 10.13)
  * AppleClang 10.0.1 (macOS 10.14)
  * AppleClang 11.0.0 (macOS 10.15)

The build of core depends on following libraries:

  * libm         (always used; math part of libc)
  * libdl        (used only for the standalone executable)
  * libpthread   (used if certain features are enabled, see below)
  * librt        (used if certain features are enabled, see below)

Additionally, the build of tools depends on following libraries:

  * libstdc++    (used by ujit-parse-profile only for demangling C++ symbols)

If you are using Ubuntu 18.04 or Ubuntu 14.04, all dependencies
should be available from scratch.

Given the scarce list of dependencies, building and running on other Ubuntu x64
versions (or other Linux x64 distributions) should work fine as well.

For building on macOS you will additionally need Command Line Tools.

To get a better grasp of what software you'll need for building and testing,
please check with the dependency lists for Ubuntu 18.04:

  * scripts/depends-build-18-04
  * scripts/depends-test-18-04

Note: autoconf and libtool are needed only for building optional 3rd party
dependencies, which are not built by default.

Building
--------

Both in source and out of source builds are supported, the latter is
recommended. To trigger build with default configuration, please run:

 $ mkdir luavela-build
 $ cd luavela-build
 $ cmake /path/to/luavela
 $ make -j

You can add `VERBOSE=1` to the `make` command in order to get a verbose output.
The build produces a static and a shared library in one go, so CMake's
BUILD_SHARED_LIBS flag will probably be useless.

Two CLIs are built too (one that links to the static library, and the other one
with dynamic linking), it is recommended to use the statically linked version.

Libraries and binaries are created in `luavela-build/src`.

All CMake configuration flags should be passed in -DKEY=VALUE form. If KEY is a
boolean flag, any CMake-compatible boolean value can be used as VALUE (i.e.
1/0, ON/OFF, TRUE/FALSE). "ON" or "OFF" are used throughout this documentation
for consistency. Example of triggering build with some flags defined:

 $ mkdir luavela-build
 $ cd luavela-build
 $ cmake /path/to/luavela -DCMAKE_BUILD_TYPE=Debug
 $ make -j VERBOSE=1

Following configuration flags are supported:

CMAKE_BUILD_TYPE
^^^^^^^^^^^^^^^^

Type of the build. Not defined by default.

Supported values:

 * Debug   - generates binaries and libraries with all assertions enabled;
 * Release - generates binaries and libraries without assertions enabled.

For convenience, both build types include debugging information into produced
binaries.

CMAKE_INSTALL_PREFIX
^^^^^^^^^^^^^^^^^^^^

Prefix for installing uJIT into your system. We do not set any explicit default
resorting to the value provided by CMake. However, you almost definitely
want something like this for production installs:

 $ cmake /path/to/luavela -DCMAKE_INSTALL_PREFIX=/usr
 $ sudo make install

If you have a CPack module, `make package` should be used for creating a package.

UJIT_TEST_LIB_TYPE
^^^^^^^^^^^^^^^^^^

Selects which CLI (and eventually, which uJIT library) will be used
for running tests.

Supported values:

 * static - A CLI linked with the static library will be used. This is default.
 * shared - A CLI linked against the shared library will be used.

This option is currently ignored for unit tests and performance benchmarks,
they always use the static library.

UJIT_TEST_OPTIONS
^^^^^^^^^^^^^^^^^

A raw string of options to be passed to the uJIT binary during testing, e.g.:

 $ cmake /path/to/luavela -DCMAKE_BUILD_TYPE=Debug -DUJIT_TEST_OPTIONS="-Xhashf=city"

UJIT_HAS_JIT
^^^^^^^^^^^^

Enables support for the JIT compiler. "ON" by default.

UJIT_HAS_FFI
^^^^^^^^^^^^

Enables FFI support. "ON" by default.

UJIT_LUA52COMPAT
^^^^^^^^^^^^^^^^

Enables Lua 5.2 compatibility. "ON" by default.

UJIT_ENABLE_GDBJIT
^^^^^^^^^^^^^^^^^^

Enables dynamic emitting of DWARF data for assembled traces. "ON" by default.

Available only on Linux.

UJIT_ENABLE_PROFILER
^^^^^^^^^^^^^^^^^^^^

Enables uJIT sampling profiler. "ON" by default.

Requires linking with librt. Available only on Linux.

UJIT_ENABLE_IPROF
^^^^^^^^^^^^^^^^^

Enables uJIT instrumenting profiler. "ON" by default.

UJIT_ENABLE_COVERAGE
^^^^^^^^^^^^^^^^^^^^

Enables platform-level coverage support. "ON" by default.

UJIT_ENABLE_CO_TIMEOUT
^^^^^^^^^^^^^^^^^^^^^^

Enables support for coroutine timeout. "ON" by default.

Requires linking with librt. Available only on Linux.

UJIT_ENABLE_MEMPROF
^^^^^^^^^^^^^^^^^^^

Enables support for memory profiler. "ON" by default.

Requires linking with librt. Available only on Linux.

UJIT_ENABLE_THREAD_SAFETY
^^^^^^^^^^^^^^^^^^^^^^^^^

Enables protecting internal data shared by all Lua VMs. "ON" by default, i.e.
uJIT assumes that it can be executed in multi-threaded environment, multiple
Lua VMs may be created and data shared between the VMs should be properly
guarded. Use "OFF" value carefully, make 100% sure that your code creates not
more than one Lua VM.

Requires linking with libpthread.

UJIT_ENABLE_VTUNEJIT
^^^^^^^^^^^^^^^^^^^^

Enables instrumenting the code with Intel VTune JIT API for correct profiling
assembled traces. "OFF" by default.

Available only on Linux.

UJIT_USE_VALGRIND
^^^^^^^^^^^^^^^^^

Enables support of Valgrind. Must be set for correct behaviour of Callgrind
as well. "OFF" by default.

UJIT_PROTECT_MCODE
^^^^^^^^^^^^^^^^^^

Enables protection of memory pages with enabled machine code, so that none of
them are writable and executable at the same time. Disable only if the page
protection twiddling becomes a bottleneck. Protection is "ON" by default.
Use "OFF" value at your own risk. See src/jit/lj_mcode.c for more details.

Testing
-------

Please note that more dependencies are need for running the full test suite
compared to just building the sources, and the process is not documented here.
However, it is possible to run the very basic sanity tests using

 $ make tests_smoke
