#!/usr/bin/perl
#
# Tests for compilation of concatenation
# Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

my $tester = UJit::Test->new(
    chunks_dir => './chunks/compiler-concat',
);

$tester->run('concat.lua', jit => 1)->exit_ok;

$tester->run('no-tbar-cse.lua', jit => 1, args => '-p-')
    ->exit_ok
    # The same TBAR instruction on both sides of the LOOP (no CSE):
    ->stdout_matches(qr/(nil\s+TBAR\s+\d+?).+-- LOOP --.+\1/s)
    ->stdout_has('out_len=1001')
;

exit;
