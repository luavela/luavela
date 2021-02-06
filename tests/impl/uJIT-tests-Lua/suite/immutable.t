#!/usr/bin/perl
#
# This is a part of uJIT's testing suite.
# Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

my $tester = UJit::Test->new(
    chunks_dir => './chunks/immutable',
);

$tester->run('immutable.lua')
    ->exit_ok
;

my @bc_recording_tests = qw/
    immutable-jit-tsets.lua
    immutable-jit-tsetb.lua
    immutable-jit-tsetv.lua
    immutable-jit-metatable.lua
/;
foreach my $chunk (@bc_recording_tests) {
    $tester->run($chunk, args => '-p-')
        ->exit_not_ok
        ->exit_without_coredump
        ->stdout_has(q/gco.marked/)
        ->stderr_has(qr/attempt to modify .+? object/)
    ;
}

$tester->run('immutable-recording-simple.lua', args => '-p-')
    ->exit_ok
    # check that recording is supported
    ->stdout_has_no(qr/TRACE.+?abort.+?NYI/)
    ->stdout_has(qr/TRACE.+?stop -> loop/)
    # but none of uj_obj_immutable calls for non-table objects were done
    ->stdout_has_no(qr/call.+?->uj_obj_immutable/)
;

$tester->run('immutable-recording.lua', args => '-p-')
    ->exit_ok
    ->stdout_has_no(qr/TRACE.+?abort.+?NYI/)
    ->stdout_has(qr/TRACE .+? stop -> loop/)
    ->stdout_has(qr/CALLS.+?uj_obj_immutable/)
    ->stdout_has(qr/call.+?->uj_obj_immutable/)
;

$tester->run('immutable-recording-mixed-types-1.lua')
    ->exit_ok
;

$tester->run('immutable-recording-mixed-types-2.lua')
    ->exit_not_ok
    ->stderr_has(qr/attempt to make immutable .+? unsupported type "thread"/)
;

$tester->run('immutable-mark-fwd.lua', args => '-p-')
    ->exit_ok
    # check that immutability guard was optimized away
    ->stdout_has_no(qr/FLOAD .+ gco.marked/)
;

$tester->run('immutable-global.lua')
    ->exit_ok
;

exit;
