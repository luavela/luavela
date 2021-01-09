#!/usr/bin/perl
#
# Tests for Lua bindings to C-level stack dumper.
# Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

my $tester = UJit::Test->new(
    chunks_dir => './chunks/dump-stack',
);

for my $jit_mode (0, 1) {
    $tester->run('frames.lua', jit => $jit_mode, lua_args => 'argFOO1')
        ->exit_ok
        ->stdout_has('P]')       # Some frames are protected
        ->stdout_has('[L')       # A Lua frame is here
        ->stdout_has('[C')       # ... and a C, too
        ->stdout_has('[M')       # ... and a metamethod, too
        ->stdout_has('CONT: C:') # ...with an aux slot
        ->stdout_has('dummy L')
        ->stdout_has('"argFOO1"')
    ;
}

exit;
