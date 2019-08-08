#!/bin/bash
#
# Test runnner for LuaJIT-tests suite.
#
# Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
# For the copyright info on the suite itself, see suite/README.

source "$(dirname `readlink -f $0`)/../../run_suite_common.sh"

$LUA_IMPL_BIN $LUA_IMPL_OPTIONS $SUITE_DIR/suite/test/test.lua

done_testing $?
