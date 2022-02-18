#!/bin/bash
#
# Test runnner for LuaJIT-tests suite.
#
# Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
# For the copyright info on the suite itself, see suite/README.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
source "$SCRIPT_DIR/../../run_suite_common.sh"

$LUA_IMPL_BIN $LUA_IMPL_OPTIONS $SUITE_DIR/suite/test/test.lua

done_testing $?
