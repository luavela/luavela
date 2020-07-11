#!/usr/bin/env bash

cmake -H. -B${BUILD_DIR} \
    -DCMAKE_BUILD_TYPE=Debug
