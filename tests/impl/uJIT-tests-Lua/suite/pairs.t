#!/usr/bin/perl
#
# Tests on pairs/next compilation.
# Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

my $tester = UJit::Test->new(
    chunks_dir => './chunks/pairs',
);

my @opts = ('-p-', '-Xitern=off -p-');
foreach my $opt (@opts) {

    $tester->run('array_hash_bound.lua', args => $opt)
        ->exit_ok
        ->stdout_has(q/TRACE/)
    ;

    $tester->run('array_middle.lua', args => $opt)
        ->exit_ok
        ->stdout_has(q/TRACE 1 stop -> loop/)
        ->stdout_has(q/TRACE 2 stop -> loop/)
        ->stdout_has_no(q/TRACE 8/) # limit number of traces
    ;

    $tester->run('hash_middle.lua', args => $opt)
        ->exit_ok
        ->stdout_has(q/TRACE 1 stop -> loop/)
        ->stdout_has_no(q/TRACE 4/) # limit number of traces
    ;

    $tester->run('hash_last.lua', args => $opt)
        ->exit_ok
        ->stdout_has(qr/TRACE 1 abort.*NYI/)
    ;

    $tester->run('array_last_no_hash.lua', args => $opt)
        ->exit_ok
        ->stdout_has(qr/TRACE 1 abort.*NYI/)
    ;

    $tester->run('nil_key.lua', args => $opt)
        ->exit_ok
        ->stdout_has(q/TRACE 1 stop -> loop/)
        ->stdout_has(q/TRACE 2 stop -> loop/)
    ;

    $tester->run('nil_key_no_array.lua', args => $opt)
        ->exit_ok
        ->stdout_has(q/TRACE/)
    ;

    # Reduced test case from MAD testsuite that was failed during development
    # of next recording many times.
    # No traces will be generated, just check if it runs ok
    $tester->run('from_mad.lua', args => $opt)
        ->exit_ok
    ;

    $tester->run('self_modify_1.lua', args => $opt)
        ->exit_ok
        ->stdout_has(q/TRACE 1 stop -> loop/)
        ->stdout_has(q/TRACE 2 stop -> loop/)
        ->stdout_has_no(q/TRACE 4/) # limit number of traces
    ;

    $tester->run('self_modify_2.lua', args => $opt)
        ->exit_ok
    ;
}

$tester->run('array_hash_bound_1.lua', args => '-Xitern=off -p-')
    ->exit_ok
    ->stdout_has(q/TRACE/)
;
$tester->run('array_hash_bound_1.lua', args => '-p-')
    ->exit_ok
    ->stdout_has(q/TRACE 1 stop -> loop/)
;

$tester->run('array_hash_bound_2.lua', args => '-Xitern=off -p-')
    ->exit_ok
    ->stdout_has(q/TRACE 1 stop -> loop/)
    ->stdout_has(q/TRACE 2 stop -> loop/)
    ->stdout_has_no(q/TRACE 4/) # limit number of traces
;
$tester->run('array_hash_bound_2.lua', args => '-p-')
    ->exit_ok
    ->stdout_has(q/TRACE 1 stop -> loop/)
    ->stdout_has(q/TRACE 3 stop -> loop/)
    ->stdout_has_no(q/TRACE 5/) # limit number of traces
;

$tester->run('self_modify.lua', args => '-Xitern=off -p-')
    ->exit_ok
    ->stdout_has(q/TRACE 1 stop -> loop/)
    ->stdout_has(q/TRACE 2 stop -> loop/)
    ->stdout_has(q/TRACE 3 stop -> loop/)
    ->stdout_has_no(q/TRACE 5/) # limit number of traces
;
$tester->run('self_modify.lua', args => '-p-')
    ->exit_ok
    ->stdout_has(q/TRACE 1 stop -> loop/)
    ->stdout_has(q/TRACE 3 stop -> loop/)
    ->stdout_has(q/TRACE 4 stop -> loop/)
    ->stdout_has_no(q/TRACE 6/) # limit number of traces
;

# test on BC_ISNEXT/BC_JITRNL despcecialization
$tester->run('isnext.lua', args => '-p-')
    ->exit_ok
    ->stdout_has(q/JITRNL/)
    ->stdout_has(q/ITERL/)
;
