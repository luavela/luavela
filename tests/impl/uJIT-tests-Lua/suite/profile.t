#!/usr/bin/perl
#
# Generic tests for profiler.
# Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

my $tester = UJit::Test->new(
    chunks_dir => './chunks/profile',
);

for my $jit_mode (0, 1) {
    $tester->run('profile.lua', jit => $jit_mode)
        ->exit_ok
        ->stdout_has('74999652500252')
    ;
}

exit;
