#!/usr/bin/perl
#
# Tests for Lua bindings to C-level compiler-related dumpers (trace IR and
# generated machine cide).
# Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

my $tester = UJit::Test->new(
    chunks_dir => './chunks/dump-compiler',
);

$tester->run('bindings.lua', jit => 1)
    ->exit_ok
    ->stdout_has('TRACE 1 IR')
    ->stdout_has('------------ LOOP ------------')
    ->stdout_has('SNAP')
    ->stdout_has('TRACE 1 mcode')
    ->stdout_has('-> LOOP:')
    ->stdout_has('->LOOP')
    ->stdout_has('->0')
    ->stdout_has('->1')
;

$tester->run('bindings.lua', jit => 0)
    ->exit_ok
    ->stdout_has_no('TRACE 1 IR')
    ->stdout_has_no('TRACE 1 mcode')
;

#
# This test covers only limited part of dumping functionality as tracing and
# assembling traces is not a determenistic process.
#

$tester->run('arith.lua', jit => 1, args => '-p-')
    ->exit_ok
    ->stdout_has(qr/\bADD\b/)   ->stdout_has(qr/\baddsd\b/)
    ->stdout_has(qr/\bSUB\b/)   ->stdout_has(qr/\bsubsd\b/)
    ->stdout_has(qr/\bMUL\b/)   ->stdout_has(qr/\bmulsd\b/)
    ->stdout_has(qr/\bDIV\b/)   ->stdout_has(qr/\bdivsd\b/)
    ->stdout_has(qr/\bMOD\b/)
    ->stdout_has(qr/\bPOW\b/)   ->stdout_has(q/math.pow/)
    ->stdout_has(qr/\bNEG\b/)
    ->stdout_has(qr/\bABS\b/)   ->stdout_has(q/math.abs/)
                                ->stdout_has(q/math.min/)
    ->stdout_has(qr/\bLDEXP\b/) ->stdout_has(q/math.ldexp/)
    ->stdout_has(qr/\bMIN\b/)   ->stdout_has(q/math.min/)
    ->stdout_has(qr/\bMAX\b/)   ->stdout_has(q/math.max/)
    ->stdout_has(qr/\bFPMATH\b/)->stdout_has(q/math.sin/)->stdout_has(qr/\bfsin\b/)
    ->stdout_has(qr/\bADDOV\b/)
    ->stdout_has(qr/\bSUBOV\b/)
;

$tester->run('assertions.lua', jit => 1, args => '-p-')
    ->exit_ok
    ->stdout_has(qr/\bLT\b/)
    ->stdout_has(qr/\bLE\b/)
    ->stdout_has(qr/\bULT\b/)
    ->stdout_has(qr/\bULE\b/)
    ->stdout_has(qr/\bEQ\b/)
    ->stdout_has(qr/\bNE\b/)
    ->stdout_has(qr/\bABC\b/)
    ->stdout_has(qr/\bSNAP\b/)
;

$tester->run('bitwise.lua', jit => 1, args => '-p-')
    ->exit_ok
    ->stdout_has(qr/\bBNOT\b/)
    ->stdout_has(qr/\bBSWAP\b/)
    ->stdout_has(qr/\bBAND\b/)
    ->stdout_has(qr/\bBOR\b/)
    ->stdout_has(qr/\bBXOR\b/)
    ->stdout_has(qr/\bBSHL\b/)
    ->stdout_has(qr/\bBSHR\b/)
    ->stdout_has(qr/\bBROL\b/)
    ->stdout_has(qr/\bBROR\b/)
;

$tester->run('calls.lua', jit => 1, args => '-p-')
    ->exit_ok
;

$tester->run('memrefs.lua', jit => 1, args => '-p-')
    ->exit_ok
    ->stdout_has('CALLL  lj_tab_len')
    ->stdout_has(qr/\bALOAD\b/)
    ->stdout_has(qr/\bHLOAD\b/)
    ->stdout_has(qr/\bFLOAD\b/)
    ->stdout_has(qr/\bSLOAD\b/)
    ->stdout_has(qr/\bASTORE\b/)
    ->stdout_has(qr/\bHSTORE\b/)
    ->stdout_has(qr/\bAREF\b/)
    ->stdout_has(qr/\bHREFK\b/)
    ->stdout_has(qr/\bNEWREF\b/)
;

$tester->run('strings.lua', jit => 1, args => '-p-')
    ->exit_ok
    ->stdout_has(qr/\bSTRREF\b/)
;

$tester->run('upvalues.lua', jit => 1, args => '-p-')
    ->exit_ok
    ->stdout_has(qr/\bUREFO\b/)
;

my $bad_fname = $tester->non_writable_fname;
$tester->run('upvalues.lua', jit => 1, args => "-p$bad_fname") # Dump to bad path
    ->exit_not_ok
    ->exit_without_coredump
;

#
# Programmatic dumping compiler's progress
#

$tester->run('progress.lua', jit => 1)
    ->exit_ok
    ->stdout_has(qr/TRACE\s+\d+\s+start/)
    ->stdout_has(qr/TRACE\s+\d+\s+IR/)
    ->stdout_has(qr/TRACE\s+\d+\s+mcode/)
    ->stdout_has(qr/TRACE\s+\d+\s+stop/)
    ->stdout_has(qr/TRACE\s+\d+\s+exit/)
    ->stdout_has(qr/TRACE\s+\d+\s+abort.+?NYI: FastFunc string\.char/)
;
