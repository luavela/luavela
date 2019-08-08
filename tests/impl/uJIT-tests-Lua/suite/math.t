#!/usr/bin/perl
#
# Extra tests for math functions, cases not covered by the Lua 5.1 suite
# Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

my $chunks_dir = './chunks/math';

my $tester = UJit::Test->new(
    chunks_dir => $chunks_dir,
);

$tester->run('coercion.lua', jit => 0)->exit_ok;

$tester->run('frexp.lua', jit => 1)->exit_ok;
$tester->run('frexp.lua', jit => 0)->exit_ok;

$tester->run('fp-classify.lua', jit => 0)->exit_ok;

# JIT is turned off because of the following bug:
# https://www.freelists.org/post/luajit/bug-report,4
$tester->run('modf.lua', jit => 0)->exit_ok;

$tester->run('nan.lua', jit => 0)->exit_ok;

$tester->run('jithtrig.lua', jit => 1, args => '-p-', lua_args => "$chunks_dir")
    ->exit_ok
    # Make sure that all functions are tested
    ->stdout_has(qr/CALLN.+?sinh/)
    ->stdout_has(qr/CALLN.+?cosh/)
    ->stdout_has(qr/CALLN.+?tanh/)
    # Coercion is tested
    ->stdout_has(qr/flt STRTO/)
    # No NYIs
    ->stdout_has_no(qr/NYI: FastFunc math.+/)
;

$tester->run('jitatrig.lua', jit => 1, args => '-p-', lua_args => "$chunks_dir")
    ->exit_ok
    # Make sure that all functions are tested
    ->stdout_has(qr/FUNCC .+ math.asin/)
    ->stdout_has(qr/FUNCC .+ math.acos/)
    ->stdout_has(qr/FUNCC .+ math.atan/)
    # asin and acos are calculated by atan2 too
    ->stdout_has(qr/flt ATAN2/)
    ->stdout_has(qr/fpatan/)
    # Coercion is tested
    ->stdout_has(qr/flt STRTO/)
    # No NYIs
    ->stdout_has_no(qr/NYI: FastFunc math.+/)
;

$tester->run('jitexp.lua', jit => 1, args => '-p-')
    ->exit_ok
    ->stdout_has(qr/flt FPMATH .+ exp\b/)
    ->stdout_has(q/call/)
;
$tester->run('jitexp2.lua', jit => 1, args => '-p-')
    ->exit_ok
    ->stdout_has(qr/flt FPMATH .+ exp2/)
    ->stdout_has(q/call/)
;
$tester->run('jit-fp-classify.lua', jit => 1, args => '-p-', lua_args => "$chunks_dir")
    # Used for recording of isnzero and isfinite
    ->stdout_has(qr/flt FPAND/)
    # Used for recording of isinf
    ->stdout_has(qr/flt ABS .+ nan/)
    # Traces with coercions are present
    ->stdout_has(qr/call .+uj_str_tonum/)
    # String didn't get folded as constant in coercion checks
    ->stdout_has(qr/str TOSTR/)
    ->stdout_has(qr/flt STRTO/)
    # Only two traces are generated: one for 'for' loop, one for jit.off()
    ->stdout_has(qr/TRACE 2/)
    ->stdout_has_no(qr/TRACE 3/)
    ->exit_ok
;
$tester->run('jitpow.lua', jit => 1, args => '-p-')
    ->exit_ok
    # ujit records numerical POW(a, b) as EXP2(b * LOG2(A))
    ->stdout_has(qr/flt FPMATH .+ exp2/)
    ->stdout_has(qr/flt FPMATH .+ log2/)
    # but then rejoins it to libm 'pow' call
    ->stdout_has(q/call/)
;
$tester->run('jitpowi.lua', jit => 1, args => '-p-')
    ->exit_ok
    ->stdout_has(q/flt POW/)
    ->stdout_has(qr/call 0x/) # call to lj_vm_powi
;
$tester->run('jitpows.lua', jit => 1, args => '-p-')
    ->exit_ok
    ->stdout_has(qr/flt FPMATH .+ exp2/)
    ->stdout_has(qr/flt FPMATH .+ log2/)
    ->stdout_has(q/call/)
;
$tester->run('jitpowsqrt.lua', jit => 1, args => '-p-')
    ->exit_ok
    ->stdout_has_no(q/flt POW/)
    ->stdout_has_no(qr/flt FPMATH .+ exp2/)
    ->stdout_has_no(qr/flt FPMATH .+ log2/)
    ->stdout_has(qr/flt FPMATH .+ sqrt/)
    ->stdout_has_no(q/call/)
;
$tester->run('jitpowsimple.lua', jit => 1, args => '-p-')
    ->exit_ok
    ->stdout_has_no(q/flt POW/)
    # we have 2^x in tests and LOG2(2) should be folded
    ->stdout_has_no(qr/flt FPMATH .+ log2/)
    ->stdout_has(qr/flt FPMATH .+ exp2/)
    ->stdout_has(q/flt MUL/)
    ->stdout_has(q/flt LDEXP/)
;
exit;
