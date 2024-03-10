#!/bin/bash
#
# This is a part of uJIT's testing suite.
# Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

CURRENT_DIR=`pwd`
UJIT_BIN=${CURRENT_DIR}/../src/ujit

IPONWEB_TESTS="${CURRENT_DIR}/iponweb"
PERF_TESTS="${IPONWEB_TESTS}/perf"
PERF_TESTS_LAPI="${IPONWEB_TESTS}/perf/lapi"
LUA_CHUNKS="${IPONWEB_TESTS}/../impl/uJIT-tests-Lua/suite/chunks"

EXIT_SUCCESS=0
EXIT_FAILURE=1
EXIT_STATUS=$EXIT_SUCCESS

# Setting to make 'printf' output double-s gracefully.
LC_NUMERIC="en_US.UTF-8"

LUA_PATH="?;?.lua;${PERF_TESTS_LAPI}/?.lua;"

function run_test
{
    pushd ${PERF_TESTS}

    start=$(date +%s.%N)

    LUA_PATH="${LUA_PATH};$3" $1 "$2" 1>$1.stdout

    if [[ $? -eq 0 ]]; then
        duration=$(echo "$(date +%s.%N) - $start" | bc)
        printf "Total CPU time for $2: %.3fs\n" ${duration}
    else
        echo "FAIL $1 $2"
        EXIT_STATUS=$EXIT_FAILURE
    fi

    popd
}

if [ ! -d ${CURRENT_DIR}/../tests ]
then
	echo "No tests dir at path \"${CURRENT_DIR}/..\"."
	exit 1
fi

# NOTE: tests are relative to ${CURRENT_DIR}.
run_test "tests-run/test_table_traversal" "capi/chunks/luae_iterate.lua"
run_test "tests-run/test_table_traversal" "capi/chunks/lua_next.lua"
run_test "${UJIT_BIN}" "lapi/table.lua" "${LUA_CHUNKS}/table/?.lua"

exit $EXIT_STATUS
