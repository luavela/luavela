#!/usr/bin/perl
#
# Tests for uJIT profile parser.
# Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;

# for in-source runs
use lib '../../tests/impl/uJIT-tests-Lua/suite/lib';

use UJit::Test;
use Cwd;

# Let's generate a tiny fake stream for each case and try to parse them.
sub run_case
{
    my ($tester, $case) = @_;

    my $name = $case->{name};
    my $out_dir = $tester->dir("profile_parser_$name");
    my $profile_fname = "$out_dir/profile.bin";

    # Mocker should always succeed:
    my $mocker_args = sprintf('%s --%s', $profile_fname, $name);
    $tester->run_tool(UJit::Test::PROF_MOCKER, $name,
        args     => $mocker_args,
        out_dir  => $out_dir,
        valgrind => $case->{no_valgrind} ? undef : {
            strict => 1,
        },
    )->exit_ok("$name: mock");

    my $parser_args = sprintf('--profile %s --callgraph %s',
        $profile_fname, "$out_dir/profile.cg",
    );
    my $parser_result = $tester->run_tool(UJit::Test::PROF_PARSER, $name,
        args     => $parser_args,
        out_dir  => $out_dir,
        valgrind => $case->{no_valgrind} ? undef : {
            strict => $case->{expect_ok},
        },
    );

    if ($case->{expect_ok}) {
        $parser_result
            ->exit_ok("$name: parse, good exit")
            ->stdout_matches(qr/
                \A
                uJIT \s profile \s \| \s UJP \s event \s stream \s v4      \s*\n
                profile_id     \s+ = \s 0x[0-9a-f]+                        \s*\n
                interval       \s+ = \s \d++ \s usec                       \s*\n
                loaded_so_num  \s+ = \s \d++                               \s*\n
                start_time     \s+ = \s
                    \d{4}-\d{2}-\d{2} \s \d{2}:\d{2}:\d{2} \s UTC          \s*\n
                end_time       \s+ = \s
                    \d{4}-\d{2}-\d{2} \s \d{2}:\d{2}:\d{2} \s UTC          \s*\n
                duration       \s+ = \s
                    \d+ \s minute\(s\), \s \d{1,2} \s second\(s\)          \s*\n
                num_samples    \s+ = \s \d+                                \s*\n
                num_overruns   \s+ = \s \d+                                \s*\n
                overruns_share \s+ = \s (?:\S+)                            \s*\n
                \n
                Loaded \s shared \s objects:                               \s*\n
                \+-++\+-++\+-{12}\+-{12}\+                                 \s*\n
                \| \s NAME \s+ \| \s BASE \s+ \| \s LOADED \s+ \|
                    \s \(UNUSED\) \s+ \|                                   \s*\n
                # Information about shared objects
                (?:.+)
                \+-++\+-++\+-{12}\+-{12}\+                                 \s*\n
                \n
                vmstate \s counters:                                       \s*\n
                \+-++\+-++\+-{12}\+-{12}\+                                 \s*\n
                \| \s NAME \s+ \| \s COUNT \s+ \| \s PERCENTAGE \s+ \|
                    \s ABSOLUTE \s+ \|                                     \s*\n
                # Information about vmstate counters
                (?:.+)
                \+-++\+-++\+-{12}\+-{12}\+                                 \s*\n
                \n
                # etc
                .++
                \Z
            /amsx)
        ;
    } else {
        $parser_result
            ->exit_not_ok("$name: parse, bad exit")
            ->exit_without_coredump("$name: parse, no coredump")
        ;
    }
}

my $cwd        = Cwd::cwd();
my $tools_dir  = $ENV{TOOLS_BIN_DIR} // "$cwd/..";
my $chunks_dir = $ENV{CHUNKS_DIR} // "$tools_dir/tests/chunks";

my $tester = UJit::Test->new(
    tools_dir => $tools_dir,
    chunks_dir => $chunks_dir
);

my @cases = (
    { # Good binary
        name      => 'all',
        expect_ok => 1,
    },
    { # ffid > FF__MAX
        name      => 'wrongffunc',
        expect_ok => 0,
    },
    { # Just an empty stream
        name      => 'empty',
        expect_ok => 0,
    },
    { # No bottom frame in callgraph
        name      => 'nobottom',
        expect_ok => 0,
    },
    { # Several MAIN_LUA frames
        name      => 'mainlua',
        expect_ok => 1,
    },
    { # Streamd and marked lfuncs in pair.
        name      => 'markedlfunc',
        expect_ok => 1,
    },
    { # Mocked host VM states should be parsed consistently
        name      => 'hvmstates',
        expect_ok => 1,
    },
    # Miscached Lua function (when there is a marked lfunc entry without any
    # symbol previously streamed).
    {
        name      => 'lfunc_miscached',
        expect_ok => 0,
    },
    { # Same function, but different names.
        name      => 'lfunc_diffnames',
        expect_ok => 0,
    },
    { # Same functions, but different lines in code.
        name      => 'lfunc_difflines',
        expect_ok => 0,
    },
    { # Miscached trace.
        name      => 'trace_miscached',
        expect_ok => 0,
    },
    { # Traces with same IDs and generation, but have different symbols.
        name      => 'trace_diffnames',
        expect_ok => 0,
    },
    # Traces with same ID, name and generation, but different lines.
    {
        name      => 'trace_difflines',
        expect_ok => 0,
    },
    # Special case when lfunc or trace is already streamed with explicit symbol
    # (see UJP_FRAME_EXPLICIT SYMBOL in ujp_write.c).
    {
        name      => 'duplicates',
        expect_ok => 1,
    },
    { # Lua function "demangling".
        name      => 'lua_demangle',
        expect_ok => 1,
    },
    { # Nonexistent Lua file.
        name      => 'lua_demangle_bad_file',
        expect_ok => 1,
    },
    { # Streamed function's line is out of the file.
        name      => 'lua_demangle_wrong_line',
        expect_ok => 1,
    },
    { # 'function' keyword is not found.
        name      => 'lua_demangle_no_func',
        expect_ok => 1,
    },
    { # NB! valgrind must be disabled since it doesn't support vDSO-related stuff.
        name        => 'vdso',
        expect_ok   => 1,
        no_valgrind => 1,
    },
);

run_case($tester, $_) foreach @cases;
