#!/usr/bin/perl
#
# Some tests for places in our platform which use leb128 utility module.
# Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

my $tester = UJit::Test->new(
    chunks_dir => './chunks/leb128',
);

my $ref_out_fname = './chunks/leb128/bc-dump-reference.out';
open my $FH_ref_out, '<', $ref_out_fname
    or die("Unable to open reference output $ref_out_fname: $!");
my $ref_out = do { local $/; <$FH_ref_out> };
close $FH_ref_out;

# If something is wrong with leb128, variable name gets corrupted:
$tester->run('syntax-error.lua')
    ->exit_not_ok
    ->exit_without_coredump
    ->stderr_has(q/var050/)
;

#
# Tests for parsing, serializing and deserializing chunks with lots of
# variable names and string constants.
#

$tester->run('bc-dump.lua')
    ->exit_ok
    ->stdout_eq($ref_out)
    ->stderr_eq('')
;

exit;
