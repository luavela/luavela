#!/usr/bin/perl -w
#
# Tests for uJIT memprof parser.
# Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;

# for in-source runs
use lib '../../tests/impl/uJIT-tests-Lua/suite/lib';

use UJit::Test;
use Cwd;

my $cwd        = Cwd::cwd();
my $tools_dir  = $ENV{TOOLS_BIN_DIR} // "$cwd/..";
my $ujit_dir    = $ENV{UJIT_BIN_DIR} // "$cwd/../../src/";
my $ujit_bin    = "$ujit_dir/ujit";

my $tester = UJit::Test->new(
    tools_dir => $tools_dir,
    ujit_dir => $ujit_dir
);

# NB! Later will find a nicer way to handle mocking:
my $fname_stub  = "$cwd/ujit-memprof.bin";
my $mocker_name = 'ujit-mock-memprof.lua';
my $memprof     = `$ujit_bin $tools_dir/$mocker_name $fname_stub`;
chomp($memprof);

die "Unable to mock a memprof" unless $memprof =~ /^\Q$fname_stub\E/;

$tester->run_tool(
    UJit::Test::MEMPROF_PARSER, 'smoke_run', args => '--help'
)->exit_ok;

$tester->run_tool(UJit::Test::MEMPROF_PARSER, 'output_check',
    args => sprintf('--profile %s', $memprof))
    ->exit_ok
    # Following depend on line numbering in the mocker, refactor with care:
    ->stdout_has(qr/\Q$mocker_name\E.+?line 8\b/)
    ->stdout_has(qr/\Q$mocker_name\E.+?line 11\b/)
    ->stdout_has(qr/\Q$mocker_name\E.+?line 12\b/)
    # Test correct order of the output, refactor with care:
    ->stdout_matches(qr/
        ALLOCATIONS.+
            \Q$mocker_name\E:5.+? 100.+
            \Q$mocker_name\E:5.+? 100.+
            \Q$mocker_name\E:5.+? 60.+
        DEALLOCATIONS.+
            \Q$mocker_name\E:5.+? 50.+
            Overrides.+
                \Q$mocker_name\E:5,\sline\s8
    /sx)
;

unlink $memprof;
