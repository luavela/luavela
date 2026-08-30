#!/usr/bin/perl
#
# Tests for compilation of concatenation
# Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;
use Test::More;

if ($ENV{'UJIT_CINTERP'} && $ENV{'UJIT_CINTERP'} eq 'ON') {
    plan skip_all => 'Relies on JIT support';
}

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
