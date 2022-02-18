#!/bin/bash
#
# Smoke tests for uJIT.
#
# Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

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
    local tnum=$1
    local tdsc=$2

    $ECHO "${COLOR_GREEN}PASS${NO_COLOR} [$tnum $tdsc]"
}

function test_fail
{
    local tnum=$1
    local tdsc=$2
    local message=$3

    $ECHO "${COLOR_RED}FAIL${NO_COLOR} [$tnum $tdsc] $message"
    exit 1 # Fine to just abort the whole suite run
}

function test01_hello_world
{
    local tnum="01"
    local tdsc="hello world"
    local tout=$SUITE_OUT_DIR/test$tnum.out
    local terr=$SUITE_OUT_DIR/test$tnum.err

    $LUA_IMPL_BIN $LUA_IMPL_OPTIONS \
        -e 'local s = "world"; _G.print("Hello " .. s)' \
        1>$tout 2>$terr

    if [[ $? != 0 ]]; then
        test_fail "$tnum" "$tdsc" 'Successful exit expected'
    fi

    if [[ "$(cat $tout)" != 'Hello world' ]]; then
        test_fail "$tnum" "$tdsc" "Unexpected output in $tout"
    fi

    if [[ -s $terr ]]; then
        test_fail "$tnum" "$tdsc" "Empty stderr expected, see $terr"
    fi

    test_pass "$tnum" "$tdsc"
}

function test02_bad_module
{
    local tnum="02"
    local tdsc="bad module load"
    local tout=$SUITE_OUT_DIR/test$tnum.out
    local terr=$SUITE_OUT_DIR/test$tnum.err

    echo "? Lua syntax error" >$SUITE_OUT_DIR/bad_module.lua

    cd $SUITE_OUT_DIR
        $LUA_IMPL_BIN $LUA_IMPL_OPTIONS \
            -lbad_module -e 'print("Unreachable")' \
            1>$tout 2>$terr

        if [[ $? == 0 ]]; then
            test_fail "$tnum" "$tdsc" 'Unsuccessful exit expected'
        fi
    cd ~-

    if [[ -s $tout ]]; then
        test_fail "$tnum" "$tdsc" "Empty stdout expected, see $tout"
    fi

    if ! grep -q 'unexpected symbol near' $terr; then
        test_fail "$tnum" "$tdsc" "Expected error message not found, see $terr"
    fi

    if grep -q 'PANIC' $terr; then
        test_fail "$tnum" "$tdsc" "Error handling may be broken, see $terr"
    fi

    test_pass "$tnum" "$tdsc"
}

function test03_numeric_loop
{
    local tnum="03"
    local tdsc="simple numeric loop"
    local tout=$SUITE_OUT_DIR/test$tnum.out
    local terr=$SUITE_OUT_DIR/test$tnum.err

    $LUA_IMPL_BIN $LUA_IMPL_OPTIONS -p- -e \
        'print("jit=" .. tostring(jit and jit.status() or false)); local s = 0; for i = 1, 100 do s = s + i end; print("result=" .. tostring(s))' \
        1>$tout 2>$terr

    if [[ $? != 0 ]]; then
        test_fail "$tnum" "$tdsc" 'Successful exit expected'
    fi

    if [[ -s $terr ]]; then
        test_fail "$tnum" "$tdsc" "Empty stderr expected, see $terr"
    fi

    if ! grep -q 'result=5050' $tout; then
        test_fail "$tnum" "$tdsc" 'The result of the Lua script is incorrect'
    fi

    if grep -q 'jit=true' $tout; then
        if ! grep -q 'TRACE 1 mcode' $tout; then
            test_fail "$tnum" "$tdsc" "JIT progress log not found, see $tout"
        fi
    fi

    test_pass "$tnum" "$tdsc"
}

test01_hello_world
test02_bad_module
test03_numeric_loop

done_testing $?
