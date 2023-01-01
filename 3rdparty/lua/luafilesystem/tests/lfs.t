#!/usr/bin/perl
#
# Test for luafilesystem used in luacheck.
# Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;

use lib '../../../../tests/impl/uJIT-tests-Lua/suite/lib';

use UJit::Test qw/:tools/;

my $tester = UJit::Test->new(
    chunks_dir => './chunks',
    ujit_dir => '../../../../src',
);

`pwd`;

$tester->run('test.lua', env => {'LUA_CPATH' => '../../luafilesystem/?.so'})
    ->exit_ok
    ->stdout_has('LuaFileSystem 1.7.0')
;

exit;
