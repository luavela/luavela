#!/usr/bin/perl
#
# Tests for memory profiler.
# Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

my $tester = UJit::Test->new(
    chunks_dir => './chunks/memprof',
);

$tester->run('memprof.lua')
    ->exit_ok
    ->stdout_has('table')
;

$tester->run('duration.lua')->exit_ok;

exit;
