#!/usr/bin/perl
#
# Tests sink optimization.
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
    chunks_dir => './chunks/compiler-sink',
);

$tester->run('nosink.lua', args => '-p-')
    ->exit_ok
    ->stdout_has_no(q/{sink}/)
;

$tester->run('sink.lua', args => '-p-')
    ->exit_ok
    ->stdout_has(q/{sink}/)
    ->stdout_has_no(q/lj_tab_new/)
    ->stdout_has_no(q/lj_tab_dup/)
;

$tester->run('partsinkouter.lua', args => '-p-')
    ->exit_ok
    ->stdout_has(qr/{sink} .+ TDUP/)
    ->stdout_has(qr/{sink} .+ TNEW/)
;

$tester->run('partsinkstore.lua', args => '-p-')
    ->exit_ok
    ->stdout_has(q/{sink}/)
;
