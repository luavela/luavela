#!/usr/bin/env bash
#
# OS-specific dependencies installer for Travis CI, which is run during
# the `install` step.
# Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

if [ $TRAVIS_OS_NAME = "linux" ]; then
    os_version=$(lsb_release -sr | tr . -)

    sudo xargs -a "scripts/depends-build-${os_version}" apt install -y
    sudo xargs -a "scripts/depends-test-${os_version}" apt install -y
    pip3 install --user -r scripts/requirements-tests.txt
fi
