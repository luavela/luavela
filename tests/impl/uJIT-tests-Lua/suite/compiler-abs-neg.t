#!/usr/bin/perl
#
# Constants for fast calculation of IR_NEG and IR_ABS
# are stored as valid 128-bit TValue's. Behaviour of the test chunk
# should be equal with JIT on and off. Besides, dumping IR code should
# Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

my $tester = UJit::Test->new(
    chunks_dir => './chunks/compiler-abs-neg',
);

$tester->run('abs-neg.lua', jit => 0)->exit_ok;

# NB! In general, asserting for presense of IR_ABS/IR_NEG is somewhat fragile
# in the course of possible future optimizations etc., nut let it stay here
# for now:
$tester->run('abs-neg.lua', jit => 1, args => '-p-')
    ->exit_ok
    ->stdout_has(qr/\bABS\b/)
    ->stdout_has(qr/\bNEG\b/)
;
