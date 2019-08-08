#!/usr/bin/perl
#
# Choosing __lt metamethod of the second operand
# if __lt for first operand is nil.
# Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

my $tester = UJit::Test->new(
    chunks_dir => './chunks/meta-comp',
);

$tester->run('meta-comp-lt.lua')
    ->exit_ok
    ->stdout_has(qr/\b__lt\b/)
    ->stdout_has(qr/\btrue\b/)
;
