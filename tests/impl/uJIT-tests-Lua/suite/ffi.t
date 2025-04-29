#!/usr/bin/perl
#
# Tests for FFI machinery and related patches.
# Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

sub _run_tests_group {
    my ($extlib_func_name, $tester, @tests) = @_;

    foreach my $test (@tests) {
        my %asserts = %{ $test->{asserts} };

        my $run_result = $tester->run($test->{file},
                                      lua_args => $extlib_func_name);

        while (my($check, $expr_list) = each %asserts) {
            if (scalar @{ $expr_list}) {
                $run_result->$check($_) for (@{ $expr_list });
            } else {
                $run_result->$check();
            }
        }
    }
}

sub run_tests {
    my ($tester, $tests) = @_;

    while (my ($extlib_func_name, $tests_group) = each %{ $tests }) {
        _run_tests_group($extlib_func_name, $tester, @{ $tests_group });
    }
}

my $tester = UJit::Test->new(
    chunks_dir => './chunks/ffi',
);

my @tarray = (
    'gcfin_table_reallocation.lua',
);

$tester->run($_)->exit_ok() for (@tarray);

exit;
