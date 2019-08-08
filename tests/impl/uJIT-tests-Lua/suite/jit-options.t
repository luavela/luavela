#!/usr/bin/perl
#
# This is a part of uJIT's testing suite.
# Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

use constant {
    NOHREFK_CHUNK_NAME => 'nohrefk.lua',
    NORETL_CHUNK_NAME  => 'noretl.lua',
    JITCAT_CHUNK_NAME  => 'jitcat.lua',
};

my $tester = UJit::Test->new(
    chunks_dir => './chunks/jit-options',
);

#
# Test presence/absence of HREFK optimization
#

my @opt_hrefk_emitted = (
    '-p-',            # Default optimization mode
    '-p- -O-nohrefk', # Explicitly turned off
);

foreach my $opt (@opt_hrefk_emitted) {
    $tester->run(NOHREFK_CHUNK_NAME, args => $opt)
        ->exit_ok
        ->stdout_has(q/tab.hmask/)
        ->stdout_has(q/HREFK/)
    ;
}

my @opt_hrefk_not_emitted = (
    '-p- -Onohrefk',  # Explicitly turned on, spelling #1
    '-p- -O+nohrefk', # Explicitly turned on, spelling #2
);

foreach my $opt (@opt_hrefk_not_emitted) {
    $tester->run(NOHREFK_CHUNK_NAME, args => $opt)
        ->exit_ok
        ->stdout_has_no(q/tab.hmask/)
        ->stdout_has_no(q/HREFK/)
    ;
}

$tester->run(NOHREFK_CHUNK_NAME, jit => 0)
    ->exit_not_ok
    ->exit_without_coredump
    ->stderr_has(q/JIT must be on/)
;

#
# Test presence/absence of returns to lower frames
#

my @opt_retf_emitted = (
    '-p-',           # Default optimization mode
    '-p- -O-noretl', # Explicitly turned off
);

foreach my $opt (@opt_retf_emitted) {
    $tester->run(NORETL_CHUNK_NAME, args => $opt)
        ->exit_ok
        ->stdout_has(q/RETF/)
    ;
}

my @opt_retf_not_emitted = (
    '-p- -Onoretl',  # Explicitly turned on, spelling #1
    '-p- -O+noretl', # Explicitly turned on, spelling #2
);

foreach my $opt (@opt_retf_not_emitted) {
    $tester->run(NORETL_CHUNK_NAME, args => $opt)
        ->exit_ok
        ->stdout_has_no(q/RETF/)
        ->stdout_has(qr/abort.+-- return to lower frame prohibited/)
    ;
}

$tester->run(NORETL_CHUNK_NAME, jit => 0)
    ->exit_not_ok
    ->exit_without_coredump
    ->stderr_has(q/JIT must be on/)
;

#
# Test presence/absence of concatenation compilation
#

my @opt_jitcat_emitted = (
    '-p-',           # Default optimization mode
    '-p- -O-jitcat', # Explicitly turned off
);

foreach my $opt (@opt_jitcat_emitted) {
    $tester->run(JITCAT_CHUNK_NAME, args => $opt)
        ->exit_ok
        ->stdout_has_no(q/BUFPUT/)
        ->stdout_has_no(q/BUFSTR/)
        ->stdout_has(q/compiling concatenation is disabled/)
    ;
}

my @opt_jitcat_not_emitted = (
    '-p- -Ojitcat',  # Explicitly turned on, spelling #1
    '-p- -O+jitcat', # Explicitly turned on, spelling #2
);

foreach my $opt (@opt_jitcat_not_emitted) {
    $tester->run(JITCAT_CHUNK_NAME, args => $opt)
        ->exit_ok
        ->stdout_has(q/BUFPUT/)
        ->stdout_has(q/BUFSTR/)
    ;
}

$tester->run(JITCAT_CHUNK_NAME, jit => 0)
    ->exit_not_ok
    ->exit_without_coredump
    ->stderr_has(q/JIT must be on/)
;

exit;
