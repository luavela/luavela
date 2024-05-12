# Setup environment on Linux

Action encapsulates the workflow for LuaVela testing routine (configuration for
the specified flavor, building, running smoke tests and running the whole
testing routine).

## How to use Github Action from Github workflow

Add the following code to the running steps before uJIT configuration:
```
- uses: ./.github/actions/run-tests
  with:
    cmakeflags: >-
      -DCMAKE_BUILD_TYPE=Debug
      -DUJIT_ENABLE_COVERAGE=OFF
      -DUJIT_ENABLE_CO_TIMEOUT=OFF
      -DUJIT_ENABLE_GDBJIT=OFF
      -DUJIT_ENABLE_IPROF=OFF
      -DUJIT_ENABLE_MEMPROF=OFF
      -DUJIT_ENABLE_PROFILER=OFF
      -DUJIT_ENABLE_THREAD_SAFETY=OFF
      -DUJIT_HAS_FFI=OFF
      -DUJIT_HAS_JIT=OFF
      -DUJIT_LUA52COMPAT=OFF
    libtype: static
    toolchain: cmake/toolchain/Clang.cmake
    testopts: -O4
```
