#!/bin/bash
#
# Test runnner for CERN-MAD-tests suite.
#
# Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
#
# For the copyright info on the suite itself, see suite/*.lua and
# https://github.com/MethodicalAcceleratorDesign/MAD/blob/dev/README.md.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
source "$SCRIPT_DIR/../../run_suite_common.sh"

if [[ -z "$TEST_FILES" ]]; then
    TEST_FILES="luacore.lua luagmath.lua luaobject.lua luaunitext.lua"
fi

cd $SUITE_DIR/suite

$SUITE_PROVE_J -e "$LUA_IMPL_BIN $LUA_IMPL_OPTIONS" $TEST_FILES
done_testing $?
