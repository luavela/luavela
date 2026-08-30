#!/bin/bash
#
# This is a part of uJIT's testing suite.
# Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

ARCH=$(uname -m)

if [[ "$ARCH" == "x86_64" ]]; then
    FLAGS="-M intel"
fi

objdump -d $FLAGS $1 | ./scripts/cinterp_tailcalls_verifier.py
