#!/usr/bin/perl
#
# Extra tests for error messages.
# Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

my $tester = UJit::Test->new(
    chunks_dir => './chunks/errors',
);

$tester->run('errors.lua')->exit_ok;
