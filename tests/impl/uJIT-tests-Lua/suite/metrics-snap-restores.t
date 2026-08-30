#!/usr/bin/perl
#
# Some tests for places in our platform which use leb128 utility module.
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
