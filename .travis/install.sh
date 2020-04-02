#!/bin/bash
#
# OS-specific dependencies installer for Travis CI, which is run during
# the `install` step.
# Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

if [ $TRAVIS_OS_NAME = "linux" ]; then
    sudo xargs -a scripts/depends-build-18-04 apt install -y
    sudo xargs -a scripts/depends-test-18-04 apt install -y
    pip3 install --user -r scripts/requirements-tests.txt
fi
