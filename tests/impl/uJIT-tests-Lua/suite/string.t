#!/usr/bin/perl
#
# Tests for string functions.
# Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

my $tester = UJit::Test->new(
    chunks_dir => './chunks/string',
);

# ujit.string.* tests

$tester->run('jit-trim.lua', args => '-p-')
    ->exit_ok
    ->stdout_has_no(qr/TRACE.+?abort.+?/)
    ->stdout_has(qr/uj_str_trim/)
    ->stdout_has(qr/TRACE.+?stop -> loop/);

$tester->run('find.lua')->exit_ok;
$tester->run('trim.lua')->exit_ok;
$tester->run('split.lua')->exit_ok;

# string.find tests

$tester->run('find-upper-lower-recording.lua', args => '-p-')
    ->exit_ok
    ->stdout_has_no(qr/TRACE.+?abort.+?/)
    ->stdout_has(qr/TRACE.+?stop -> loop/);

$tester->run('find-side-exit.lua', args => '-p-')
    ->exit_ok
    ->stdout_has_no(qr/TRACE.+?abort.+?/)
    ->stdout_has(qr/TRACE.+?stop -> loop/)
    # no specialization on pattern argument, thus only one trace
    ->stdout_has_no(q/TRACE 2/);

$tester->run('find-init-arg.lua', args => '-p-')
    ->exit_ok
    ->stdout_has_no(qr/TRACE.+?abort.+?/)
    ->stdout_has(q/TRACE/);

$tester->run('find-plain.lua', args => '-p-')
    ->exit_ok
    ->stdout_has_no(qr/TRACE.+?abort.+?/)
    ->stdout_has(qr/TRACE.+?stop -> loop/)
    ->stdout_has_no(q/TRACE 2/)
    ->stdout_has_no(qr/>\s*tru\s*.LOAD/); # no type-guarded loads on 'true'

$tester->run('find-plain-load.lua', args => '-p-')
    ->exit_ok
    ->stdout_has(qr/TRACE.+?stop -> loop/)
    ->stdout_has(qr/>\s*tru\s*.LOAD/)  # type-guarded load on 'true'
    ->stdout_has(qr/>\s*fal\s*.LOAD/); # and then type-guarded load on 'false'

# string.format tests

$tester->run('format.lua')->exit_ok;

$tester->run('bufput-fuse.lua', args => '-p-')
    ->exit_ok
    ->stdout_has(qr/TRACE.+?stop -> loop/)
    ->stdout_has_no(qr/TRACE.+?abort.+?/)
    ->stdout_has(q/TOSTR/)             # Has TOSTR IR
    ->stdout_has_no(q/uj_str_fromint/) # but string creation is fused by BUFPUT
    ->stdout_has_no(q/uj_str_fromnumber/)
    ->stdout_has(q/uj_sbuf_push_int/)  # by means of uj_sbuf_push_int
    ->stdout_has(q/uj_sbuf_push_number/) # and uj_sbuf_push_number.
    ->stdout_has(q/SNEW/)              # Has SNEW IR
    ->stdout_has_no(q/uj_str_new/)     # but string creation is fused by BUFPUT
    ->stdout_has(q/uj_sbuf_push_block/) # by means of uj_sbuf_push_block.
    ->stdout_has_no(q/uj_sbuf_push_str/) # Push of constant single-char string
    ->stdout_has(q/uj_sbuf_push_char/) # was replaced with uj_sbuf_push_char.
;

$tester->run('bufstr-cse.lua', args => '-p-')
    ->exit_ok
    ->stdout_has(qr/TRACE.+?stop -> loop/)
    ->stdout_has_no(qr/TRACE.+?abort.+?/)
    # string created only on pre-LOOP
    ->stdout_matches(qr/uj_str_frombuf.+LOOP:/s)
    ->stdout_matches_no(qr/LOOP:.+uj_str_frombuf/s)
;

$tester->run('bufput-const-fold.lua', args => '-p-')
    ->exit_ok
    ->stdout_has(qr/TRACE.+?stop -> loop/)
    ->stdout_has_no(qr/TRACE.+?abort.+?/)
    ->stdout_has(q/-foo-bar-84/)  # constant folding happened
;

$tester->run('bufhdr-append.lua', args => '-p-')
    ->exit_ok
    ->stdout_has(qr/TRACE.+?stop -> loop/)
    ->stdout_has_no(qr/TRACE.+?abort.+?/)
    ->stdout_has(q/APPEND/)  # BUFHDR APPEND was emitted
;

$tester->run('bufstr-const-fold.lua', args => '-p-')
    ->exit_ok
    ->stdout_has(qr/TRACE.+?stop -> loop/)
    ->stdout_has_no(qr/TRACE.+?abort.+?/)
    ->stdout_has_no(q/BUFSTR/)  # all BUFSTRs were folded away
;

$tester->run('find-fold-bug.lua', args => '-p-')
    ->exit_ok
    ->stdout_has_no(qr/TRACE.+?abort.?/)
;
