#!/usr/bin/perl
#
# Support for -X key=value in CLI.
# Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
#

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

my $tester = UJit::Test->new(
    chunks_dir => './chunks/cli-X',
);

# -X hashf=...
$tester->run('any.lua', args => '-h')
    ->exit_not_ok('-X documented in -h')
    ->exit_without_coredump
    ->stderr_has('-X k=v');

$tester->run('any.lua', args => '-Xhashf=murmur')
    ->exit_ok('Well-formed: -Xk=v')
;

$tester->run('any.lua', args => '-X hashf=murmur')
    ->exit_ok('Well-formed: -X k=v')
;

$tester->run('any.lua', args => '-Xhashf=murmur -X hashf=murmur')
    ->exit_ok('Well-formed: -Xk=v -X k=v')
;

$tester->run('any.lua', args => '-X')
    ->exit_not_ok('Missing key and value')
    ->exit_without_coredump
    ->stderr_has('Unknown option')
;

$tester->run('any.lua', args => '-Xhashf')
    ->exit_not_ok('Missing value: -Xk')
    ->exit_without_coredump
    ->stderr_has('Unknown option')
;

$tester->run('any.lua', args => '-Xhashf=')
    ->exit_not_ok('Missing value: -Xk=')
    ->exit_without_coredump
    ->stderr_has('Unknown value')
;

$tester->run('any.lua', args => '-Xhashf_=murmur')
    ->exit_not_ok('Unsupported key which looks like supported')
    ->exit_without_coredump
    ->stderr_has('Unknown option')
;

$tester->run('any.lua', args => '-Xhashf=unsupported')
    ->exit_not_ok('Unsupported value')
    ->exit_without_coredump
    ->stderr_has('Unknown value')
;

$tester->run('any.lua', args => '-Xhashf=murmur_')
    ->exit_not_ok('Unsupported value which looks like supported')
    ->exit_without_coredump
    ->stderr_has('Unknown value')
;

# Smoke testing other hashf's:

# TODO: Proper testing should be implemented later
$tester->run('any.lua', args => '-Xhashf=city')->exit_ok('Well-formed: -Xk=v');

# -X itern=...
$tester->run('any.lua', args => '-Xitern=disable')
    ->exit_not_ok('Unsupported value')
    ->exit_without_coredump
    ->stderr_has('Unknown value')
;

$tester->run('any.lua', args => '-Xitern=off -b-')
    ->exit_ok
    ->stdout_has('ITERC')
    ->stdout_has_no('ITERN')
;

$tester->run('any.lua', args => '-Xitern=on -b-')
    ->exit_ok
    ->stdout_has('ITERN')
    ->stdout_has_no('ITERC')
;
