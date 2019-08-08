#!/usr/bin/perl
#
# Call to getfenv(0) should be compiled.
# Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

my $tester = UJit::Test->new(
    chunks_dir => './chunks/compiler-getfenv',
);

$tester->run('getfenv.lua', jit => 0)->exit_ok;

# NB! In general, asserting presense of particular IR ins is somewhat fragile
# in the course of possible future optimizations etc., but let it stay here
# for now:
$tester->run('getfenv.lua', jit => 1, args => '-p-')
    ->exit_ok
    ->stdout_has(qr/\bthr LREF\b/)
    ->stdout_has(qr/\bFLOAD.+?thread\.env\b/)
;
