#!/usr/bin/perl
#
# Tests on platform-level coverage.
# Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;

use Digest::CRC qw(crc32);
use Test::Deep;

use lib './lib';
use UJit::Test;

my $chunks_dir = './chunks/coverage';

my $tester = UJit::Test->new(
    chunks_dir => $chunks_dir,
);

sub get_lines
{
    my $fname = shift;
    open my $FH, '<', $fname;
    chomp(my @lines = <$FH>);
    close $FH;
    return \@lines;
}

sub arrays_to_hash
{
    my $columns = shift;
    my $values = shift;
    return {map {$columns->[$_] => $values->[$_]} 0 .. $#$values};
}

sub get_coverage
{
    my $fname = shift;
    my $lines = get_lines($fname);
    my $coverage = [
        map {
            arrays_to_hash([qw(file_path line file_path_crc)],
                [split(/\t/, $_)])
        } @$lines
    ];
    return $coverage;
}

my $test1 = "$chunks_dir/coverage_simple.lua";
my $crc1 = crc32($test1);
my $test2 = "$chunks_dir/coverage_middle_jump.lua";
my $crc2 = crc32($test2);
my $test3 = "$chunks_dir/coverage_pause.lua";
my $crc3 = crc32($test3);
my @deeptests = (
    {
        file => $test1,
        lhc => 'cov1.lhc',
        expected => [
            {
                file_path => $test1,
                line => 7,
                file_path_crc => $crc1,
            },
            (
                map {
                    {
                        file_path     => '',
                        line          => $_,
                        file_path_crc => $crc1,
                    }
                } (9, 10, 11, 10, 11, 10, 11, 10, 13)
            ),
        ],
    },
    {
        file => $test2,
        lhc => "cov2.lhc",
        expected => [
            {
                file_path => $test2,
                line => 9,
                file_path_crc => $crc2,
            },
            (
                map {
                    {
                        file_path     => '',
                        line          => $_,
                        file_path_crc => $crc2,
                    }
                } (11, 13, 14, 15, 16, 11, 14, 15, 16, 11, 14, 15, 16, 7, 8)
            ),
        ],
    },
    {
        file => $test3,
        lhc => "cov3.lhc",
        expected => [
            {
                file_path => $test3,
                line => 5,
                file_path_crc => $crc3,
            },
	    {
	        file_path => '',
		line => 11,
		file_path_crc => $crc3,
	    }
        ],
    }
);

foreach (@deeptests) {
    $tester->run('wrapper.lua', lua_args => "$_->{file} $_->{lhc}")->exit_ok;
    my $out = get_coverage($_->{lhc});
    cmp_deeply($out, $_->{expected}, "Coverage output is ok");
    unlink $_->{lhc};
}

my @oktests = qw(coverage_gc.lua coverage_varinfo.lua coverage_if_then.lua);
foreach my $chunk (@oktests) {
    $tester->run('wrapper.lua', lua_args => "$chunks_dir/$chunk $chunk.lhc")
        ->exit_ok;
    unlink "$chunk.lhc";
}

$tester->run('exclude.lua', lua_args => "$test1 exclude.lhc cov")
    ->exit_ok
    ->stdout_has('started: true');
# Coverage output should be empty
cmp_deeply(get_coverage("exclude.lhc"), [], "Coverage with exclude is ok");
unlink "exclude.lhc";

# Test that ujit correctly handles invalid regexp
$tester->run('exclude.lua', lua_args => "$test1 exclude.lhc [[:foo]]")
    ->exit_ok
    ->stdout_has('started: false');
unlink "exclude.lhc";

# Test that ujit correctly handles read-only file permisssions
my $badfname = $tester->non_writable_fname;
$tester->run('exclude.lua', lua_args => "$test1 $badfname 123")
    ->exit_ok
    ->stdout_has('started: false');

