#!/bin/bash
#
# Run stock Lua 5.1 testing suite.
#
# Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
#
# For the copyright info on the suite itself, see suite/README

# Some constants for coloured output by run_test
ECHO="echo -e"

if [ -t 1 ]; then # Produce colored output in terminal only
    COLOR_GREEN='\033[0;32m'
    COLOR_RED='\033[0;31m'
    COLOR_YELLOW='\033[1;33m'
    NO_COLOR='\033[0m'
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
source "$SCRIPT_DIR/../../run_suite_common.sh"

# This is passed by CMake, if not present - we're launching
# from the shell
if [[ -z "${TEST_LIB_NAMES}" ]]; then
    TEST_LIB_NAMES="lib1 lib11 lib2 lib21"
fi

# Copy tests scripts other files
cp -a $SUITE_DIR/suite/. $SUITE_OUT_DIR

# Check that shared libs are built and
# copy them to $SUITE_OUT_DIR/libs
if [[ `uname` == "Darwin" ]]; then
    SHARED_LIB_EXT="dylib"
else
    SHARED_LIB_EXT="so"
fi
for TEST_LIB in $TEST_LIB_NAMES; do
    TEST_LIB_PATH=$SUITE_BIN_DIR/$TEST_LIB.$SHARED_LIB_EXT
    if [ -f $TEST_LIB_PATH ]; then
        ln -s $TEST_LIB_PATH $SUITE_OUT_DIR/libs
    else
        $ECHO "${COLOR_RED}FAIL${NO_COLOR} Test library $TEST_LIB_PATH is not built - run 'make Lua-5.1-tests'"
        exit 1
    fi
done

# Environment setup.
# For more information see lua5.1/README.
export LUA_INIT="package.path = '?;'..package.path"

PATH_ORIGINAL=$PATH
LUA_JIT_ON="lua $LUA_IMPL_OPTIONS"
LUA_JIT_OFF="lua $LUA_IMPL_OPTIONS -j off"
TESTS_FAILED=
TEST_LOG_PREFIX=$SUITE_OUT_DIR/test_run

function run_test()
{
    local lua_cmd=$1
    local lua_chunk=$2
    local log_fname="$TEST_LOG_PREFIX-$lua_chunk.log"

    $lua_cmd $lua_chunk &>$log_fname
    if [[ $? -eq 0 ]]; then
        $ECHO "${COLOR_GREEN}PASS${NO_COLOR} $1 $2"
    else
        $ECHO "${COLOR_RED}FAIL${NO_COLOR} $1 $2"
        TESTS_FAILED="$TESTS_FAILED $1 $2; "
    fi
}

function skip_test()
{
    $ECHO "${COLOR_YELLOW}SKIP${NO_COLOR} $1 $2: $3"
}

pushd $SUITE_OUT_DIR
    # Since our binary is not called lua and there is no installable,
    # synthetic lua binary is prepared to be called from tests.
    rm -f ./lua && ln -s $LUA_IMPL_BIN ./lua

    # Tests also need a copy of "lib2.so" as "-lib2.so"
    cp ./libs/lib2.so ./libs/-lib2.so

    export PATH=.

    $ECHO "Running the entire suite with JIT compiler off"
    run_test "$LUA_JIT_OFF" all.lua

    $ECHO "Running selected tests with JIT compiler on"
    run_test "$LUA_JIT_ON" main.lua
    run_test "$LUA_JIT_ON" constructs.lua
    run_test "$LUA_JIT_ON" literals.lua
    run_test "$LUA_JIT_ON" strings.lua
    run_test "$LUA_JIT_ON" nextvar.lua
    run_test "$LUA_JIT_ON" locals.lua
    run_test "$LUA_JIT_ON" calls.lua
    run_test "$LUA_JIT_ON" sort.lua
    run_test "$LUA_JIT_ON" files.lua
    run_test "$LUA_JIT_ON" verybig.lua
    run_test "$LUA_JIT_ON" events.lua
    run_test "$LUA_JIT_ON" math.lua
    run_test "$LUA_JIT_ON" vararg.lua
    run_test "$LUA_JIT_ON" attrib.lua
    run_test "$LUA_JIT_ON" errors.lua
    run_test "$LUA_JIT_ON" pm.lua

    skip_test "$LUA_JIT_ON" big.lua  "designed to run from an external Lua wrapper"
    skip_test "$LUA_JIT_ON" closure.lua "known to fail when JIT is on"
    skip_test "$LUA_JIT_ON" gc.lua   "known to fail when JIT is on"
    skip_test "$LUA_JIT_ON" db.lua   "known to fail when JIT is on"
    skip_test "$LUA_JIT_ON" api.lua  "implementation-specific tests"
    skip_test "$LUA_JIT_ON" code.lua "implementation-specific tests"
popd

export PATH=$PATH_ORIGINAL

if [[ -n "$TESTS_FAILED" ]]; then
    echo "====================================================================="
    echo "Following Lua 5.1 tests failed (full commands):"
    echo $TESTS_FAILED
    echo "See logs ($TEST_LOG_PREFIX*.log) for more details"
    echo "====================================================================="
    exit 1
fi

touch $SUITE_OK_FILE
exit 0
