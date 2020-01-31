#!/bin/bash
#
# This is a part of uJIT's testing suite.
# Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
source "$SCRIPT_DIR/../../run_suite_common.sh"

if [[ -z "${PROFILE_PARSER}" ]]; then
    PROFILE_PARSER=$SUITE_DIR/../../../tools/ujit-parse-profile
fi

cd $SUITE_OUT_DIR

ln -s $SUITE_BIN_DIR/test_* .
cp -a $SUITE_DIR/suite/chunks .

PROFILE_PARSER=${PROFILE_PARSER} $SUITE_PROVE_J ./test_*

done_testing $?
