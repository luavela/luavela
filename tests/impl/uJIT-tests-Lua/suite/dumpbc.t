#!/usr/bin/perl
#
# Tests for Lua bindings to C-level bytecode dumpers for dumping single
# instructions as well as whole Lua functions.
# Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;
use Test::More;

my $chunks_dir = './chunks/dumpbc';
my $tester     = UJit::Test->new(
    chunks_dir => $chunks_dir,
);

# Run reference dump (byte-to-byte equal to output of the original bc.lua)
my $ref_dump_fname = "$chunks_dir/reference-dump.bc";
open my $FH_ref_dump, '<', $ref_dump_fname
    or die("Unable to open refernce bc dump $ref_dump_fname");
my $ref_dump = do { local $/; <$FH_ref_dump> };
close $FH_ref_dump;
$tester->run('chunk.lua')
    ->exit_ok
    ->stdout_eq($ref_dump)
;

$tester->run('try-overflow-hint-buffer.lua')
    ->exit_ok
    ->stdout_has(qr/UGET\s+.+; var_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx~/)
    ->stdout_has(qr/USETS\s+.+; var_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx~ ; "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"~/)
    ->stdout_has(qr/KSTR\s+.+"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"~/)
    ->stdout_has(q/"\t\n\r\014\127\127\127\127\127\127\127"~/)
;

my $long_chunk_fname = 'long-chunk-name-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.lua';
$tester->run($long_chunk_fname)
    ->exit_ok
   ->stdout_has(qr/FNEW\s+.+; ~x{35}\.lua:\d/)
   ->stdout_has(qr/-- ~x{35}\.lua:\d-\d/)
;

$tester->run('bad-arg-num.lua')
    ->exit_not_ok
    ->exit_without_coredump
    ->stderr_has(qr/userdata expected, got no value/)
;

$tester->run('bad-arg-1.lua')
    ->exit_not_ok
    ->exit_without_coredump
    ->stderr_has(q/userdata expected/)
;

$tester->run('bad-arg-2.lua')
    ->exit_not_ok
    ->exit_without_coredump
    ->stderr_has(qr/function expected/i)
;

$tester->run('fast-function.lua')
    ->exit_ok
    ->stdout_has(qr/0000\s+?FUNCC.+?math\.sin/)
;

my $chunk_fname = 'dumpbcins.lua';
$tester->run($chunk_fname)
    ->exit_ok
    ->stdout_has(q/bad argument/)
    ->stdout_has(q/userdata expected/)
    ->stdout_has(q/math.sin/)
    ->stdout_has(q/Invalid instruction index/)
    ->stdout_has(qr{FUNCF.+?;\s+$chunks_dir/$chunk_fname:[1-9]})
    ->stdout_has(qr{FUNCV.+?;\s+$chunks_dir/$chunk_fname:[1-9]})
    ->stdout_has(qr/\d{4}\s+KSTR/)
;

#
# Testing command line options (-b does not prevent chunk from being executed)
#

my $assume_has_bytecode  = qr/(?:-- BYTECODE --)|GGET|JMP/;

$tester->run('dumpbcins.lua', args => '-b-')
    ->exit_ok
    ->stdout_has($assume_has_bytecode)
    ->stdout_has(q/math.sin/)
;

my $inline_chunk = 'x=0;for i=1,1E6 do x=x+i end;print(x)';
$tester->run($inline_chunk,
    inline => 1,
    args   => '-b-',
)
    ->exit_ok
    ->stdout_has($assume_has_bytecode)
    ->stdout_has('(command line)')
    ->stdout_has(q/500000500000/)
;

#
# Testing -B (dump bc and source)
#

# Test that -B functions correctly
# 1) Prototype's source gets printed too
# 2) Case when bytecode's source line is less than previously printed one is handled
$tester->run('source-dump-test-chunk.lua', args => '-B-')
    ->exit_ok
    ->stdout_has($assume_has_bytecode)
    ->stdout_has(q/local y = arg/) # source of "f" prototype got printed
    ->stdout_has(q/local inc = 5/) # source of "f2" prototype got printed
    ->stdout_has(q/y = y + inc/)   # consecutive lines of source get printed
;

# Test that trying to print source of inline chunk issues a warning and bytecode still gets printed
$tester->run($inline_chunk,
    inline => 1,
    args   => '-B-',
)
    ->exit_ok
    ->stdout_has($assume_has_bytecode)
    ->stderr_has('warning: source will not be printed in a dump (command line chunk was passed)')
;

# Testing that printing long lines doesn't overflow line buffer and prints long line prefixes
$tester->run('long-source-lines.lua', args => '-B-')
    ->exit_ok
    ->stdout_has($assume_has_bytecode)
    ->stdout_has(qr/local x = 1$/)
    ->stdout_has(qr/yy.*y = 2$/)
    ->stdout_has(qr/zz.*zy\Q[...]\E$/)
;

exit;
