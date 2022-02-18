#!/usr/bin/perl
#
# Tests for strhash metrics
# Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

my $tester = UJit::Test->new(
    chunks_dir => './chunks/metrics-strhash',
);

$tester->run('strhash.lua', jit => 0)->exit_ok;

exit;
