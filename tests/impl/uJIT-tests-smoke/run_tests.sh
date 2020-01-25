#!/bin/bash
#
# Smoke tests for uJIT.
#
# Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
source "$SCRIPT_DIR/../../run_suite_common.sh"

# Some constants for coloured output:
ECHO="echo -e"

if [ -t 1 ]; then # Produce colored output in terminal only
    COLOR_GREEN='\033[0;32m'
    COLOR_RED='\033[0;31m'
    NO_COLOR='\033[0m'
fi

function test_pass
{
    local tprx=$1

    $ECHO "${COLOR_GREEN}PASS${NO_COLOR} $tprx"
}

function test_fail
{
    local tprx=$1
    local message=$2

    $ECHO "${COLOR_RED}FAIL${NO_COLOR} $tprx $message"
    exit 1 # Fine to just abort the whole suite run
}

function test01_hello_world
{
    local tnum="01"
    local tout=$SUITE_OUT_DIR/test$tnum.out
    local terr=$SUITE_OUT_DIR/test$tnum.err
    local tprx="[test$tnum]"

    $LUA_IMPL_BIN $LUA_IMPL_OPTIONS \
        -e 'local s = "world"; _G.print("Hello " .. s)' \
        1>$tout 2>$terr

    if [[ $? != 0 ]]; then
        test_fail $tprx 'Successful exit expected'
    fi

    if [[ "$(cat $tout)" != 'Hello world' ]]; then
        test_fail $tprx "Unexpected output in $tout"
    fi

    test_pass $tprx
}

function test02_error_handling
{
    local tnum="02"
    local tout=$SUITE_OUT_DIR/test$tnum.out
    local terr=$SUITE_OUT_DIR/test$tnum.err
    local tprx="[test$tnum]"

    echo "? Lua syntax error" >$SUITE_OUT_DIR/bad_module.lua

    cd $SUITE_OUT_DIR
        $LUA_IMPL_BIN $LUA_IMPL_OPTIONS \
            -lbad_module -e 'print("Unreachable")' \
            1>$tout 2>$terr

        if [[ $? == 0 ]]; then
            test_fail $tprx 'Unsuccessful exit expected'
        fi
    cd ~-

    if [[ -s $tout ]]; then
        test_fail $tprx "Empty stdout expected, see $tout"
    fi

    if grep -q 'PANIC' $tout; then
        test_fail $tprx "Error handling may be broken, see $terr"
    fi

    test_pass $tprx
}

function test03_jit_compiler
{
    local tnum="03"
    local tout=$SUITE_OUT_DIR/test$tnum.out
    local terr=$SUITE_OUT_DIR/test$tnum.err
    local tprx="[test$tnum]"

    $LUA_IMPL_BIN $LUA_IMPL_OPTIONS -p- -e \
        'print("jit=" .. tostring(jit.status())); local s = 0; for i = 1, 100 do s = s + i end; print("result=" .. tostring(s))' \
        1>$tout 2>$terr

    if [[ $? != 0 ]]; then
        test_fail $tprx 'Successful exit expected'
    fi

    if ! grep -q 'result=5050' $tout; then
        test_fail $tprx 'The result of the Lua script is incorrect'
    fi

    if grep -q 'jit=true' $tout; then
        if ! grep -q 'TRACE 1 mcode' $tout; then
            test_fail $tprx "JIT progress log not found, see $tout"
        fi
    fi

    test_pass $tprx
}

test01_hello_world
test02_error_handling
test03_jit_compiler

done_testing $?
