#!/usr/bin/perl
#
# Some tests for places in our platform which use leb128 utility module.
# Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

my $tester = UJit::Test->new(
    chunks_dir => './chunks/metrics-snap-restores',
);

my @chunks = qw/
loop-direct.lua
loop-side-exit.lua
loop-side-exit-non-compiled.lua
scalar.lua
/;

foreach my $chunk (@chunks) {
    $tester->run($chunk)->exit_ok;
}

exit;
