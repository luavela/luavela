#!/usr/bin/perl
#
# Generic tests for indexed loads and stores.
# This is a part of uJIT's testing suite.
# Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
#

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

my $tester     = UJit::Test->new(
    chunks_dir => './chunks/ir_indexed',
);

# Tests for handling __index metamethod.

$tester->run('load/no_index.lua', args => '-p-')
    ->exit_not_ok
    ->stdout_has("missing metamethod")
;

$tester->run('load/mt.lua', args => '-p-')
    ->exit_ok
    ->stdout_has("TRACE 1 mcode")
    ->stdout_matches(qr/
        flt.SLOAD..\#2\b.+
        str.SLOAD..\#3\b.+
        nil.SLOAD..\#4\b.+
        tru.SLOAD..\#5\b.+
        fun.SLOAD..\#6\b.+
        tab.SLOAD..\#7\b.+
        thr.SLOAD..\#8\b.+
        /xs)
;

$tester->run('load/num_mt_nil_index.lua', args => '-p-')
    ->exit_not_ok
    ->stdout_has("missing metamethod")
;

$tester->run('load/num_mt_table_index.lua', args => '-p-')
    ->exit_ok
;

$tester->run('load/loop_lookup.lua', args => '-p-')
    ->exit_not_ok
    ->stdout_has("-- looping index lookup")
;

$tester->run('load/niltvg.lua', args => '-p-')
    ->exit_ok
    ->stdout_has("TRACE 1 mcode")
    ->stdout_matches(qr/0006.+p32.+HREF.+\s.+>.+p32.+EQ.+\[0x/)
;

$tester->run('load/nonnil.lua', args => '-p-')
    ->exit_ok
    ->stdout_matches(qr/007.+p32.+AREF/)
    ->stdout_has_no("p32 EQ")
;

# Tests for handling __newindex metamethod.

$tester->run('store/no_newindex.lua', args => '-p-')
    ->exit_not_ok
    ->stdout_has("missing metamethod")
;

$tester->run('store/mt.lua', args => '-p-')
    ->exit_ok
    ->stdout_has("TRACE 1 mcode")
;

$tester->run('store/nil_newindex.lua', args => '-p-')
    ->exit_not_ok
    ->stdout_has("missing metamethod")
;

$tester->run('store/table_newindex.lua')
    ->exit_ok
    ->stdout_eq("nil\n")
;

$tester->run('store/loop_lookup.lua', args => '-p-')
    ->exit_not_ok
    ->stdout_has("-- looping index lookup")
;

$tester->run('store/nil_mm.lua', args => '-p-')
    ->exit_ok
    ->stdout_matches(qr/0012.+>.+nil.+HLOAD.+\[0x/)
;

$tester->run('store/nil_nomm_newref_conv.lua', args => '-p-')
    ->exit_ok
    # conv
    ->stdout_matches(qr/0014.+flt.+CONV.+\+100.+flt.int\s
                     0015.+p32.+NEWREF/x)
    # !conv
    ->stdout_matches(qr/0024.+p32.+NEWREF.+\"value\"\s
                     0025.+flt.+HSTORE/x)
;

$tester->run('store/nil_nomm_nonewref.lua', args => '-p-')
    ->exit_ok
    ->stdout_matches(qr/0012.+>.+tab.+EQ.+\s0013.+str.+ASTORE.+\"val\"/)
;

$tester->run('store/nonnil_mt.lua', args => '-p-')
    ->exit_ok
    ->stdout_matches(qr/0012.+>.+flt.+ALOAD.+/)
;

$tester->run('store/nonnil_nomt.lua', args => '-p-')
    ->exit_ok
    ->stdout_matches(qr/0012.+tab.+FLOAD.+tab.+meta\s
                        0013.+>.+tab.+EQ.+\[NULL\]/x)
;

exit;
