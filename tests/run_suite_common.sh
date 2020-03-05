#!/bin/bash
#
# Fine-tuning environment for running testing suites.
#
# Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

# If running from shell, set some defaults
if [[ -z "$SUITE_OUT_DIR" ]]; then
    SUITE_DIR="$(cd "$(dirname "$0")" && pwd -P)"
    SUITE_BIN_DIR=$SUITE_DIR/suite_bin
    SUITE_OUT_DIR=$SUITE_DIR/suite_run
fi

# If there is no LUA_IMPL_BIN, assume that
# we are dealing with uJIT built in-source:
if [[ -z "$LUA_IMPL_BIN" ]]; then
    LUA_IMPL_BIN=$SUITE_DIR/../../../src/ujit
fi

# Test that LUA_IMPL_BIN contains an absolute path.
# Generic handling of relative paths is tricky.
if [[ "$LUA_IMPL_BIN" != /* ]]; then
    echo "FAIL $SUITE_DIR: LUA_IMPL_BIN variable should be an absolute path."
    exit 1
fi

# Test that LUA_IMPL_BIN actually exists.
if [[ ! -f "$LUA_IMPL_BIN" ]]; then
    cat <<-ERROR_DOC
FAIL $SUITE_DIR: Executable not found in LUA_IMPL_BIN=$LUA_IMPL_BIN.

Possible solutions:

* Fix LUA_IMPL_BIN to point to the right file (an absolute path);
* If you have not built LUA_IMPL_BIN, please do so;
* Run 'make $(basename $SUITE_DIR)'.
ERROR_DOC
    exit 1
fi

# Remove artifacts of a previous run:
rm -rf $SUITE_OUT_DIR
mkdir $SUITE_OUT_DIR

# Define the path to the OK file:
SUITE_OK_FILE=$SUITE_OUT_DIR/run_tests.ok

# Define common variables and functions:

if [[ `uname` == "Darwin" ]]; then
    num_cpu=$(sysctl -n hw.logicalcpu)
else
    num_cpu=$(nproc)
fi
SUITE_PROVE_J="prove -j$num_cpu"

function done_testing()
{
    local exit_code=$1

    if [[ $exit_code != 0 ]]; then
        exit 1
    fi

    touch $SUITE_OK_FILE
    exit 0
}
