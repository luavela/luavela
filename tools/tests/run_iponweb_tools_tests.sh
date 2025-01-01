#!/bin/bash
#
# This is a part of uJIT's testing suite.
# Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
TOOLS_TESTS_DIR="$(dirname "$(readlink -f "$0")")"
TOOLS_DIR="$(realpath "$TOOLS_TESTS_DIR/..")"

if [[ -z "$TOOLS_BIN_DIR" ]]; then
    TOOLS_BIN_DIR="$TOOLS_DIR"
    TOOLS_TESTS_RUN_DIR="$TOOLS_TESTS_DIR"
else
    if [ "$TOOLS_DIR" != "$TOOLS_BIN_DIR" ]; then # out of source build
        # Need to copy tools that don't need to be built, but are expected
        # To be in the same directory as the other tools

        rm -rf "$TOOLS_BIN_DIR/parse_memprof" # remove result of previous copy
        cp -a "$TOOLS_DIR/parse_memprof" "$TOOLS_BIN_DIR"
        cp "$TOOLS_DIR/ujit-parse-memprof" "$TOOLS_BIN_DIR"

        cp "$TOOLS_DIR/ujit-mock-memprof.lua" "$TOOLS_BIN_DIR"
    fi

    TOOLS_TESTS_RUN_DIR="$TOOLS_BIN_DIR/tests"
fi

if [[ -z "$UJIT_BIN_DIR" ]]; then
    UJIT_BIN_DIR="$TOOLS_TESTS_DIR/../../src"
fi

if [[ -z "$TEST_LIB_DIR" ]]; then
    TEST_LIB_DIR="$TOOLS_DIR/../tests/impl/uJIT-tests-Lua/suite/lib"
fi

TOOLS_TESTS_OUT_DIR="$TOOLS_TESTS_RUN_DIR/tests-run"

function check_for_failure()
{
    if [[ $? -ne 0 ]]; then
        popd
        exit 1
    fi
}

if [ ! -e "$TOOLS_TESTS_RUN_DIR" ]; then
    mkdir -p "$TOOLS_TESTS_RUN_DIR"
fi

pushd "$TOOLS_TESTS_RUN_DIR"
    # remove the result of previous run
    rm -f run_iponweb_tools_tests.ok
    rm -rf "$TOOLS_TESTS_OUT_DIR"

    mkdir -p "$TOOLS_TESTS_OUT_DIR"

    env UJIT_BIN_DIR="$UJIT_BIN_DIR" \
        TOOLS_BIN_DIR="$TOOLS_BIN_DIR" \
        CHUNKS_DIR="$TOOLS_TESTS_DIR/chunks" \
    prove -j$(nproc) -I"$TEST_LIB_DIR" "$TOOLS_TESTS_DIR"/*.t
    check_for_failure

    touch run_iponweb_tools_tests.ok
popd

exit 0
