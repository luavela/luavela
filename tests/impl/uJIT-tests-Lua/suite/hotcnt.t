#!/usr/bin/perl
#
# Generic tests for hotcounting tests and JIT compiler.
# This is a part of uJIT's testing suite.
# Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
#

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;
use Test::More;

my $tester     = UJit::Test->new(
    chunks_dir => './chunks/bc_hotcnt',
);

#
# Tests for emitting HOTCNT
#

$tester->run('forl.lua', jit => 0, args => '-b-')
    ->exit_ok
    ->stdout_matches(qr/\bHOTCNT\b.+\bFORL\b.+/s)
;

$tester->run('loop_goto.lua', jit => 0, args => '-b-')
    ->exit_ok
    ->stdout_matches(qr/\bHOTCNT\b.+\bLOOP\b.+/s)
;

$tester->run('loop_while.lua', jit => 0, args => '-b-')
    ->exit_ok
    ->stdout_matches(qr/\bHOTCNT\b.+\bLOOP\b.+/s)
;

$tester->run('loop_repeat.lua', jit => 0, args => '-b-')
    ->exit_ok
    ->stdout_matches(qr/\bHOTCNT\b.+\bLOOP\b.+/s)
;

$tester->run('iterl.lua', jit => 0, args => '-b-')
    ->exit_ok
    ->stdout_matches(qr/\bHOTCNT\b.+\bITERL\b.+/s)
;

$tester->run('funcf.lua', jit => 0, args => '-b-')
    ->exit_ok
    ->stdout_matches(qr/0+1\s+HOTCNT/)
;

#
# Tests for compiler
#

# FORL
$tester->run('forl.lua', jit => 1, args => '-Ohotloop=1 -p-')
    ->exit_ok
    ->stdout_matches(qr/\bTRACE 1 mcode\b.+\bTRACE 1 stop -> loop\b.+/s)
;

$tester->run('forl.lua', jit => 1, args => '-Ohotloop=8 -p-')
    ->exit_ok
    ->stdout_matches(qr/\bTRACE 1 mcode\b.+\bTRACE 1 stop -> loop\b.+/s)
;

$tester->run('forl.lua', jit => 1, args => '-Ohotloop=9 -p-')
    ->exit_ok
    ->stdout_matches(qr/\bFORL\b.+\bTRACE 1 abort\b.+/s)
;

$tester->run('forl.lua', jit => 1, args => '-Ohotloop=10 -p-')
    ->exit_ok
    ->stdout_matches(qr/\bGGET\b.+\bTRACE 1 abort\b.+/s)
;

$tester->run('forl.lua', jit => 1, args => '-Ohotloop=11 -p-')
    ->exit_ok
    ->stdout_eq("10\n")
;

# goto LOOP
$tester->run('loop_goto.lua', jit => 1, args => '-Ohotloop=1 -p-')
    ->exit_ok
    ->stdout_matches(qr/\bTRACE 1 mcode\b.+\bTRACE 1 stop -> loop\b.+/s)
;

$tester->run('loop_goto.lua', jit => 1, args => '-Ohotloop=9 -p-')
    ->exit_ok
    ->stdout_matches(qr/\bTRACE 1 mcode\b.+\bTRACE 1 stop -> loop\b.+/s)
;

$tester->run('loop_goto.lua', jit => 1, args => '-Ohotloop=10 -p-')
    ->exit_ok
    ->stdout_matches(qr/\bTRACE 1 start\b.+\bTRACE 1 abort\b.+/s)
;

$tester->run('loop_goto.lua', jit => 1, args => '-Ohotloop=11 -p-')
    ->exit_ok
    ->stdout_eq("11\n")
;

# while LOOP
$tester->run('loop_while.lua', jit => 1, args => '-Ohotloop=1 -p-')
    ->exit_ok
    ->stdout_matches(qr/\bTRACE 1 mcode\b.+\bTRACE 1 stop -> loop\b.+/s)
;

$tester->run('loop_while.lua', jit => 1, args => '-Ohotloop=9 -p-')
    ->exit_ok
    ->stdout_matches(qr/\bTRACE 1 mcode\b.+\bTRACE 1 stop -> loop\b.+/s)
;

$tester->run('loop_while.lua', jit => 1, args => '-Ohotloop=10 -p-')
    ->exit_ok
    ->stdout_matches(qr/\bTRACE 1 start\b.+\bTRACE 1 abort\b.+/s)
;

$tester->run('loop_while.lua', jit => 1, args => '-Ohotloop=11 -p-')
    ->exit_ok
    ->stdout_eq("10\n")
;

# repeat LOOP
$tester->run('loop_repeat.lua', jit => 1, args => '-Ohotloop=1 -p-')
    ->exit_ok
    ->stdout_matches(qr/\bTRACE 1 mcode\b.+\bTRACE 1 stop -> loop\b.+/s)
;

$tester->run('loop_repeat.lua', jit => 1, args => '-Ohotloop=10 -p-')
    ->exit_ok
    ->stdout_matches(qr/\bTRACE 1 mcode\b.+\bTRACE 1 stop -> loop\b.+/s)
;

$tester->run('loop_repeat.lua', jit => 1, args => '-Ohotloop=11 -p-')
    ->exit_ok
    ->stdout_matches(qr/\bTRACE 1 start\b.+\bTRACE 1 abort\b.+/s)
;

$tester->run('loop_repeat.lua', jit => 1, args => '-Ohotloop=12 -p-')
    ->exit_ok
    ->stdout_eq("11\n")
;

# ITERL
$tester->run('iterl.lua', jit => 1, args => '-Ohotloop=1 -p-')
    ->exit_ok
    ->stdout_matches(qr/\bTRACE 1 mcode\b.+\bTRACE 1 stop -> loop\b.+/s)
;

$tester->run('iterl.lua', jit => 1, args => '-Ohotloop=3 -p-')
    ->exit_ok
    ->stdout_matches(qr/\bTRACE 1 mcode\b.+\bTRACE 1 stop -> loop\b.+/s)
;

$tester->run('iterl.lua', jit => 1, args => '-Ohotloop=4 -p-')
    ->exit_ok
    ->stdout_matches(qr/\bTRACE 1 mcode\b.+\bTRACE 1 stop -> loop\b.+/s)
;

$tester->run('iterl.lua', jit => 1, args => '-Ohotloop=5 -p-')
    ->exit_ok
    ->stdout_matches(qr/\bITERL\b.+\bTRACE 1 abort\b.+/s)
;

$tester->run('iterl.lua', jit => 1, args => '-Ohotloop=6 -p-')
    ->exit_ok
    ->stdout_matches(qr/\bTRACE 1 abort\b.+/s)
;

$tester->run('iterl.lua', jit => 1, args => '-Ohotloop=7 -p-')
    ->exit_ok
    ->stdout_eq("25\n")
;

# FUNCF
$tester->run('funcf.lua', jit => 1, args => '-Ohotloop=1 -p-')
    ->exit_ok
    ->stdout_matches(qr/\bTRACE 1 mcode\b.+\bTRACE 1 stop -> return\b.+/s)
;

$tester->run('funcf.lua', jit => 1, args => '-Ohotloop=2 -p-')
    ->exit_ok
    ->stdout_matches(qr/\bTRACE 1 mcode\b.+\bTRACE 1 stop -> return\b.+/s)
;

$tester->run('funcf.lua', jit => 1, args => '-Ohotloop=3 -p-')
    ->exit_ok
    ->stdout_matches(qr/\bTRACE 1 mcode\b.+\bTRACE 1 stop -> return\b.+/s)
;

$tester->run('funcf.lua', jit => 1, args => '-Ohotloop=4 -p-')
    ->exit_ok
    ->stdout_eq("")
;

exit;
