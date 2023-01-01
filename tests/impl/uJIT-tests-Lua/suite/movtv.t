#!/usr/bin/perl
#
# Tests for fusing load operations
# Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

my $tester = UJit::Test->new(
    chunks_dir => './chunks/movtv',
);

$tester->run('apart.lua', args => '-p-')
    ->exit_ok()
    ->stdout_has('TRACE 1 mcode')
    ->stdout_has_no('TRACE 2 ', 'single trace formed')
    ->stdout_has(qr/\d[^>]+ALOAD/, 'fused ALOAD')
    ->stdout_has_no(qr/>[^>]+?ALOAD/, 'no unfused ALOAD')
;

$tester->run('apart-scalar.lua', args => '-p-')
    ->exit_ok()
    ->stdout_has('TRACE 1 mcode')
    ->stdout_has_no('TRACE 2 ', 'single trace formed')
    ->stdout_has(qr/\d[^>]+ALOAD/, 'fused ALOAD')
    ->stdout_has_no(qr/>[^>]+?ALOAD/, 'no unfused ALOAD')
;

$tester->run('hpart-no-newref.lua', args => '-p-')
    ->exit_ok()
    ->stdout_has_no('NEWREF')
    ->stdout_has('TRACE 1 mcode')
    ->stdout_has_no('TRACE 2 ', 'single trace formed')
    ->stdout_has(qr/\d[^>]+HLOAD/, 'fused HLOAD')
    ->stdout_has_no(qr/>[^>]+?HLOAD/, 'no unfused HLOAD')
;

my $src_href = '0014';
$tester->run('hpart-no-newref-hoisting.lua', args => '-p-')
    ->exit_ok()
    ->stdout_has_no('NEWREF')
    ->stdout_has('TRACE 1 mcode')
    ->stdout_has_no('TRACE 2 ', 'single trace formed')
    ->stdout_has(qr/\d[^>]+HLOAD/, 'fused HLOAD')
    ->stdout_has_no(qr/>[^>]+?HLOAD/, 'no unfused HLOAD')
    ->stdout_has(qr/HSTORE 0016\s+$src_href/)
    ->stdout_has(qr/HSTORE 0036\s+$src_href/)
;

$tester->run('hpart-no-newref-partial.lua', args => '-p-')
    ->exit_ok()
    ->stdout_has_no('NEWREF')
    ->stdout_has('TRACE 1 mcode')
    ->stdout_has_no('TRACE 2 ', 'single trace formed')
    ->stdout_has(qr/\d[^>]+HLOAD/, 'fused HLOAD')
    ->stdout_has(qr/>[^>]+?HLOAD/, 'unfused HLOAD')
;

$tester->run('hpart-newref.lua', args => '-p-')
    ->exit_ok()
    ->stdout_has('NEWREF')
    ->stdout_has('TRACE 1 mcode')
    ->stdout_has_no('TRACE 2 ', 'single trace formed')
    ->stdout_has(qr/\d[^>]+HLOAD/, 'fused HLOAD')
    ->stdout_has_no(qr/>[^>]+?HLOAD/, 'no unfused HLOAD')
;

# If there are no guards against metatable, Lua code will fail
$tester->run('hpart-newref-meta-guard.lua', args => '-p-')
    ->exit_ok()
    ->stdout_has('NEWREF')
    ->stdout_has('TRACE 1 mcode')
    ->stdout_has_no('TRACE 2 ', 'single trace formed')
    ->stdout_has(qr/\d[^>]+HLOAD/, 'fused HLOAD')
    ->stdout_has_no(qr/>[^>]+?HLOAD/, 'no unfused HLOAD')
;

$tester->run('no-fuse-meta-src.lua', args => '-p-')
    ->exit_ok()
    ->stdout_has('TRACE 1 mcode')
    ->stdout_has('TRACE 2 mcode', 'more than trace produced')
    ->stdout_has_no(qr/\d[^>]+ALOAD/, 'fused ALOAD')
;

$tester->run('no-fuse-meta-dst.lua', args => '-p-')
    ->exit_ok()
    ->stdout_has('TRACE 1 mcode')
    ->stdout_has('TRACE 2 mcode', 'more than trace produced')
    ->stdout_has_no(qr/\d[^>]+ALOAD/, 'fused ALOAD')
;

$tester->run('fload-meta-guard.lua', args => '-p-')
    ->exit_ok()
    ->stdout_has('TRACE 1 mcode')
    ->stdout_has(qr/\d[^>]+HLOAD/, 'fused HLOAD')
    ->stdout_has_no(qr/>[^>]+?HLOAD/, 'no unfused HLOAD')
;

$tester->run('sload-meta-guard.lua', args => '-p-')
    ->exit_ok()
    ->stdout_has('TRACE 1 mcode')
    ->stdout_has(qr/\d[^>]+ALOAD/, 'fused ALOAD')
    ->stdout_has_no(qr/>[^>]+?ALOAD/, 'no unfused ALOAD')
;

$tester->run('tvload-meta-guard.lua', args => '-p-')
    ->exit_ok()
    ->stdout_has('TRACE 1 mcode')
    ->stdout_has(qr/\d[^>]+HLOAD/, 'fused HLOAD')
    ->stdout_has_no(qr/>[^>]+?HLOAD/, 'no unfused HLOAD')
;

$tester->run('in-place.lua', args => '-p-')
    ->exit_ok()
    ->stdout_has('TRACE 1 mcode')
    ->stdout_has_no('TRACE 2 mcode', 'single trace formed')
    ->stdout_has(qr/\d[^>]+ALOAD/, 'fused ALOAD')
    ->stdout_has_no(qr/>[^>]+?ALOAD/, 'no unfused ALOAD')
;

$tester->run('same-table-diff-keys.lua', args => '-p-')
    ->exit_ok()
    ->stdout_has('TRACE 1 mcode')
    ->stdout_has_no('TRACE 2 mcode', 'single trace formed')
    ->stdout_has(qr/\d[^>]+HLOAD/, 'fused HLOAD')
    ->stdout_has_no(qr/>[^>]+?HLOAD/, 'no unfused HLOAD')
;

$tester->run('same-table-same-key.lua', args => '-p-')
    ->exit_ok()
    ->stdout_has('TRACE 1 mcode')
    ->stdout_has_no('TRACE 2 mcode', 'single trace formed')
    ->stdout_has(qr/\d[^>]+HLOAD/, 'fused HLOAD')
    ->stdout_has_no(qr/>[^>]+?HLOAD/, 'no unfused HLOAD')
;

$tester->run('load-used-by-assert.lua', args => '-p-')
    ->exit_not_ok()
    ->exit_without_coredump()
    ->stdout_has('TRACE 1 mcode')
    ->stdout_has_no('TRACE 2 mcode', 'single trace formed')
    ->stdout_has(qr/\s+assert\b/)
    ->stdout_has_no('MOVTV', 'unable to apply the optimization')
;

$tester->run('load-used-by-bc-isf.lua', args => '-p-')
    ->exit_ok()
    ->stdout_has('TRACE 1 mcode')
    ->stdout_has_no('TRACE 2 mcode', 'single trace formed')
    ->stdout_has(qr/\s+ISF\s+/)
    ->stdout_has_no('MOVTV', 'unable to apply the optimization')
;

$tester->run('load-used-by-bc-ist.lua', args => '-p-')
    ->exit_ok()
    ->stdout_has('TRACE 1 mcode')
    ->stdout_has_no('TRACE 2 mcode', 'single trace formed')
    ->stdout_has(qr/\s+IST\s+/)
    ->stdout_has_no('MOVTV', 'unable to apply the optimization')
;

$tester->run('load-used-by-bc-not.lua', args => '-p-')
    ->exit_ok()
    ->stdout_has('TRACE 1 mcode')
    ->stdout_has_no('TRACE 2 mcode', 'single trace formed')
    ->stdout_has(qr/\s+NOT\s+/)
    ->stdout_has_no('MOVTV', 'unable to apply the optimization')
;

$tester->run('gset.lua', args => '-p-')
    ->exit_ok()
    ->stdout_has('TRACE 1 mcode')
    ->stdout_has_no('TRACE 2 mcode', 'single trace formed')
    ->stdout_has(qr/\s+GSET\s+/)
    ->stdout_has('MOVTV', 'optimization applied')
;

$tester->run('pri-non-nil.lua', args => '-p-')
    ->exit_ok()
    ->stdout_has('TRACE 1 mcode')
    ->stdout_has_no('TRACE 2 mcode', 'single trace formed')
    ->stdout_has(qr/\d+\s+tru HLOAD.+MOVTV\b/, 'primitive with MOVTV applied')
;

$tester->run('pri-nil-non-empty.lua', args => '-p-')
    ->exit_ok()
    ->stdout_has('TRACE 1 mcode')
    ->stdout_has_no('TRACE 2 mcode', 'single trace formed')
    ->stdout_has(qr/\d+\s+nil HLOAD.+MOVTV\b/, 'LOAD of nil with MOVTV applied')
;

$tester->run('pri-nil-empty.lua', args => '-p-')
    ->exit_ok()
    ->stdout_has('TRACE 1 mcode')
    ->stdout_has_no('TRACE 2 mcode', 'single trace formed')
    ->stdout_has(qr/\d+\s+nil HLOAD.+MOVTV\b/, 'LOAD of nil with MOVTV applied')
;
