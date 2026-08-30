#!/usr/bin/perl
#
# Generic tests for extended API.
# This is a part of uJIT's testing suite.
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
    chunks_dir => './chunks/api-extended',
);

$tester->run('api-extended.lua', jit => 0)
    ->exit_ok
;

$tester->run('api-nargs.lua', jit => 0)
    ->exit_ok
;

exit;
