#!/usr/bin/perl
#
# This is a part of uJIT's testing suite.
# Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

my $tester = UJit::Test->new(
    chunks_dir => './chunks/seal',
);

my @bc_recording_tests = qw/
    seal-without-metatable-jit-tsets.lua
    seal-without-metatable-jit-tsetb.lua
    seal-without-metatable-jit-tsetv.lua

/;
foreach my $chunk (@bc_recording_tests) {
    $tester->run($chunk, args => '-p-')
        ->exit_not_ok
        ->exit_without_coredump
        ->stdout_has(q/gco.marked/)
        ->stderr_has(qr/attempt to modify .+? object/)
    ;
}

$tester->run('seal-with-metatable-jit.lua', args => '-p-')
    ->exit_not_ok
    ->exit_without_coredump
    ->stdout_has(q/gco.marked/)
    ->stderr_has(q/in function 'setmetatable'/)
    ->stderr_has(qr/attempt to modify .+? object/)
;

$tester->run('seal.lua')
    ->exit_ok
;

$tester->run('seal-proto.lua')
    ->exit_ok
;

exit;
