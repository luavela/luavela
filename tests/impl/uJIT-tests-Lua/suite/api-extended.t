#!/usr/bin/perl
#
# Generic tests for extended API.
# This is a part of uJIT's testing suite.
# Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

my $tester = UJit::Test->new(
    chunks_dir => './chunks/api-extended',
);

$tester->run('api-extended.lua', jit => 0)
    ->exit_ok
;

$tester->run('api-nargs.lua', jit => 0)
    ->exit_ok
;

exit;
