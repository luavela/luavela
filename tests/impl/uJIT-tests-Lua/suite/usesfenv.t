#!/usr/bin/perl
#
# Tests for ujit.usesfenv
# Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

my $tester = UJit::Test->new(
    chunks_dir => './chunks/usesfenv',
);

$tester->run('usesfenv.lua', jit => 0)->exit_ok;

exit;
