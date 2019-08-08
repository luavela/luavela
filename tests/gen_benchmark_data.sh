#!/bin/bash
#
# This is a part of uJIT's testing suite.
# Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

pushd "$LUAJIT_BENCH_DIR"

mkdir -p "${OUT_DIR}"
$LUA_IMPL_BIN fasta.lua 25e5 >"${OUT_DIR}/FASTA_ujit.txt"
if [[ $? -ne 0 ]]; then
    exit 1
fi

popd

touch gen_benchmark_data.ok
exit 0
