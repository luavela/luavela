#!/bin/bash
#
# Test runnner for uJIT functional testing
#
# Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

source "$(dirname `readlink -f $0`)/../../run_suite_common.sh"

# Environment variables for Test.pm
UJIT_DIR=$(dirname -- $LUA_IMPL_BIN)
UJIT_BIN=$(basename -- $LUA_IMPL_BIN) # Test.pm only needs a filename for UJIT_BIN
UJIT_OPTIONS=$LUA_IMPL_OPTIONS

cd $SUITE_OUT_DIR

cp -a $SUITE_DIR/suite/chunks .

prove -j$(nproc) -I$SUITE_DIR/suite/lib $SUITE_DIR/suite/*.t

done_testing $?
