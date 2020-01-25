Installation
============

Prerequisites
-------------

The build is performed by CMake of version 3.3.2 or higher.

The build was tested with following compilers:

  * GCC 4.6.3 (Ubuntu 14.04)
  * GCC 4.8.4 (Ubuntu 14.04)
  * GCC 7.3.0 (Ubuntu 18.04)
  * GCC 7.4.0 (Ubuntu 18.04)
  * GCC 8.3.0 (Ubuntu 18.04)
  * Clang 3.8 (Ubuntu 14.04)
  * Clang 3.9 (Ubuntu 14.04)
  * Clang 6.0 (Ubuntu 18.04)

The build of core depends on following libraries:

  * libm         (always used; math part of libc)
  * libdl        (used only for the standalone executable)
  * libpthread   (used if certain features are enabled, see below)
  * librt        (used if certain features are enabled, see below)

Additionally, the build of tools depends on following libraries:

  * libstdc++    (used by ujit-parse-profile only for demangling C++ symbols)

If you are using Ubuntu 18.04 or Ubuntu 14.04, all dependencies
should be available from scratch.

Target environment
------------------

As of current implementation, the only supported target is Ubuntu 18.04 running
on x64 host machine. Please note, that 32-bit environment is not supported and
it will not be built correctly.

Given the scarce list of dependencies, building and running on other Ubuntu x64
versions (or other Linux x64 distributions) should work fine as well, but is not
officially supported at the moment.

To get a grasp of what software you'll need for building and testing,
please check with the dependency lists for Ubuntu 18.04:

  * scripts/depends-build-18-04
  * scripts/depends-test-18-04

Note: autoconf and libtool are needed for building 3rd party dependencies.

Building
--------

To trigger the build with default configuration, enter

 $ cmake . [...configuration flags...]
 $ make

You can add VERBOSE=1 to the make command in order to get verbose Make output.
Both a static and a shared library are built. Two CLIs are built too,
respectively, but it is recommended to use the statically linked version.
Libraries and binaries will be created in-source, i.e. 'src/ujit',
'src/libujit.a', 'src/libujit.so', etc.

All configuration flags should be passed in -DKEY=VALUE form. If KEY is a
boolean flag, any cmake-compatible boolean value can be used as VALUE (i.e.
1/0, ON/OFF, TRUE/FALSE). "ON" or "OFF" are used throughout this documentation
for consistency. Example of triggering build with some flags defined:

 $ cmake . -DCMAKE_BUILD_TYPE=Debug
 $ make VERBOSE=1

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
resorting to the value provided by cmake. However, you almost definitely
want something like this for packaging and production installs:

 $ cmake . -DCMAKE_INSTALL_PREFIX=/usr
 $ make package

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

 $ cmake . -DCMAKE_BUILD_TYPE=Debug -DUJIT_TEST_OPTIONS="-Xhashf=city"

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

UJIT_ENABLE_PROFILER
^^^^^^^^^^^^^^^^^^^^

Enables uJIT sampling profiler. "ON" by default.

Requires linking with librt.

UJIT_ENABLE_IPROF
^^^^^^^^^^^^^^^^^

Enables uJIT instrumenting profiler. "ON" by default.

UJIT_ENABLE_COVERAGE
^^^^^^^^^^^^^^^^^^^^

Enables platform-level coverage support. "ON" by default.

UJIT_ENABLE_CO_TIMEOUT
^^^^^^^^^^^^^^^^^^^^^^

Enables support for coroutine timeout. "ON" by default.

Requires linking with librt.

UJIT_ENABLE_MEMPROF
^^^^^^^^^^^^^^^^^^^

Enables support for memory profiler. "ON" by default.

Requires linking with librt.

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
