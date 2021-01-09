#!/usr/bin/perl
#
# Extra tests for os functions, cases not covered by other suites
# Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

my $tester = UJit::Test->new();

$tester->run('assert(os.getenv("FOO") == nil)', inline => 1)->exit_ok;

$tester->run('assert(os.getenv("FOO") == "BAR")',
    inline => 1,
    env => {
        FOO => 'BAR',
    },
)->exit_ok;

exit;
