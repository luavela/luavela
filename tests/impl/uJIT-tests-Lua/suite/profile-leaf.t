#!/usr/bin/perl
#
# Tests for leaf profiler.
# Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

my $tester = UJit::Test->new(
    chunks_dir => './chunks/profile-leaf',
);

my @chunks = qw/
    simple.lua
    mm-continuation.lua
    ff-vm-state.lua
    tail-call.lua tail-call-ff.lua
    coro-yield-resume.lua coro-vm-state.lua
    callgraph.lua
    restart.lua
/;

for my $jit_mode (0, 1) {
    for my $chunk (@chunks) {
        $tester->run($chunk, jit => $jit_mode)->exit_ok;
    }
}

exit;
