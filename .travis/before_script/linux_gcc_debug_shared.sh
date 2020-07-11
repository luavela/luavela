#!/usr/bin/env bash

cmake -H. -B${BUILD_DIR} \
    -DCMAKE_BUILD_TYPE=Debug \
    -DUJIT_TEST_LIB_TYPE=shared \
    -DUJIT_TEST_OPTIONS="-O4"
