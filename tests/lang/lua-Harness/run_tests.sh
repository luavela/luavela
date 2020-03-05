#!/bin/bash
#
# Test runnner for lua-Harness suite.
#
# Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
# For the copyright info on the suite itself, see suite/COPYRIGHT.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
source "$SCRIPT_DIR/../../run_suite_common.sh"

if [[ -z "$LUA_TEST_PROFILE" ]]; then
    LUA_TEST_PROFILE="profile_luajit20_compat52"
fi

cd $SUITE_OUT_DIR

# Copy tests and modules as lua-Harness assumes we're running from test_lua directory
cp -a $SUITE_DIR/suite/test_lua/. .

# NB! In theory, you can prove ... -e "$LUA_IMPL_BIN $LUA_IMPL_OPTIONS ...",
# but in practice this breaks those tests in the suite which use the binary
# ($LUA_IMPL_BIN in our case) without checking that some custom options
# may be attached to it. So skipping $LUA_IMPL_OPTIONS for now.
$SUITE_PROVE_J -e "$LUA_IMPL_BIN -l $LUA_TEST_PROFILE" *.t
done_testing $?
