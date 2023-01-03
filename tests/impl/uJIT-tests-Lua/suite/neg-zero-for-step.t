#!/usr/bin/perl
#
# This is a part of uJIT's testing suite.
# Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

my $tester = UJit::Test->new(
    chunks_dir => './chunks/neg-zero-for-step',
);

$tester->run('no-iter.lua', jit => 1)
    ->exit_ok
    ->stdout_has_no(q/OK/)
;

$tester->run('no-iter.lua', jit => 0)
    ->exit_ok
    ->stdout_has_no(q/OK/)
;

$tester->run('forever.lua', jit => 1)
    ->exit_ok
    ->stdout_has(q/1/)
    ->stdout_has(q/2000/)
;

$tester->run('forever.lua', jit => 0)
    ->exit_ok
    ->stdout_has(q/1/)
    ->stdout_has(q/2000/)
;

exit;
