#!/usr/bin/perl -w
#
# This is a tool for running performance benchmarks.
# Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use strict;
use warnings;

use Getopt::Long;

use constant {
  DEFAULT_MULTIRUN  => 1,
  DEFAULT_TEST_BASE => '.',
};

my $interp;
my $interp_ref;
my $test_base;
my $test;
my $batch;
my $multirun = DEFAULT_MULTIRUN;
my @lua_inc;
my $show_help;

my $simple_mode       = 0;
my $skip_jit_on       = 0;
my $skip_jit_off      = 0;
my $skip_jit_overhead = 0;

Getopt::Long::GetOptions(
  'interp=s'      => \$interp,
  'interp-ref=s'  => \$interp_ref,
  'test-base=s'   => \$test_base,
  'test=s'        => \$test,
  'batch=s'       => \$batch,
  'multirun=i'    => \$multirun,
  'lua-inc=s'     => \@lua_inc,
  'help'          => \$show_help,

  'simple'            => \$simple_mode,
  'skip-jit-on'       => \$skip_jit_on,
  'skip-jit-off'      => \$skip_jit_off,
  'skip-jit-overhead' => \$skip_jit_overhead,
);

$show_help = 1 if !defined $interp;

if ($show_help) {
  say <<HELP
$0 - uJIT performance test runner

SYNOPSIS

perl $0 [options]

Supported options are:

 --interp     (string):  path to the interpreter tested    [MANDATORY]
 --interp-ref (string):  path to the reference interpreter
 --test-base  (string):  path to the test base directory; if omitted:
                          * if --batch is set (see below), test base is set to
                            the location of the batch file
                          * otherwise defaults to current working directory
 --batch      (string):  path to the batch file
 --test       (string):  comma-separated list of tests to be run (if omitted, all
                         chunks from the test base or the batch file are run)
 --multirun   (integer): number of runs to take average from (default: @{[DEFAULT_MULTIRUN]})
 --lua-inc    (string):  path to directory to add to LUA_PATH (may be repeated
                         multiple times); please specify **path to directory**
                         only, correct syntax of LUA_PATH will be ensured

 --help:                 this message

Supported boolean options for testing scenarios are:

 --simple:            Runs all chunks in ref_jit_on, dev_jit_on and dev_jit_off
                      modes without asserting timings. All --skip-* flags (see
                      below) are ignored

 --skip-jit-on:       Skip test "dev version with JIT turned on
                      against ref version with JIT on"
 --skip-jit-off:      Skip test "dev version with JIT turned off
                      against ref version with JIT off"
 --skip-jit-overhead: Skip test "dev version with JIT turned on
                      against dev version with JIT off"

HELP
;
  exit;
}

if (!$simple_mode && $skip_jit_on && $skip_jit_off && $skip_jit_overhead) {
  die 'Everything is skipped: Nothing to test';
}

UJPerformanceUtils::assert_executable_file($interp);

if ($simple_mode || !($skip_jit_on && $skip_jit_off)) {
  UJPerformanceUtils::assert_executable_file($interp_ref);
}

UJPerformanceUtils::assert_file($batch) if defined $batch;

if (defined $test_base) {
  UJPerformanceUtils::assert_directory($test_base);
} else {
  if (defined $batch) {
    $test_base = UJPerformanceUtils::file_directory($batch);
  } else {
    $test_base = DEFAULT_TEST_BASE;
  }
}

my @chunks = $test? split(/\s*,\s*/, $test) : ();

push @lua_inc, $test_base; # For convenience, always add test_base to the LUA_PATH
my $LUA_PATH = sprintf 'LUA_PATH="%s;;"', join(';', map { "$_/?.lua" } @lua_inc);

my %test_env = (
  interp_dev => $interp,
  interp_ref => $interp_ref,
  lua_path   => $LUA_PATH,
  nruns      => $multirun,
  base_dir   => $test_base,
);

my $chunk_set = UJPerformanceChunkSet->new(
  test_env => \%test_env,
  batch    => $batch,
  chunks   => \@chunks,
);

if ($chunk_set->is_empty) {
  die 'No chunks to use for testing';
}

if ($simple_mode) {
  $chunk_set->test_simple;
} else {
  if (!$skip_jit_on) {
    say 'JIT on: dev against ref';
    $chunk_set->test('dev_jit_on/ref_jit_on');
    say '';
  }
  if (!$skip_jit_off) {
    say 'JIT off: dev against ref';
    $chunk_set->test('dev_jit_off/ref_jit_off');
    say '';
  }
  if (!$skip_jit_overhead) {
    say 'dev: JIT on against JIT off';
    $chunk_set->test('dev_jit_on/dev_jit_off');
    say '';
  }
}

if ($chunk_set->has_no_errors) {
  say 'PASS';
  exit 0;
}

$chunk_set->report_errors($simple_mode);

say 'FAIL';
exit 1;

package UJPerformanceUtils;

use 5.010;
use strict;
use warnings;

#
# Common file & directory assertions
#

sub assert_file {
  my $path = shift;
  die "ERROR: file does not exist: $path" unless -f $path;
}

sub assert_executable_file {
  my $path = shift;
  assert_file $path;
  die "ERROR: file is not executable $path" unless -x $path;
}

sub assert_directory {
  my $path = shift;
  die "ERROR: directory does not exist: $path" unless -d $path;
}

sub file_directory {
  my $path  = shift;
  my ($dir) = ($path =~ m|^(.*/)[^/]+$|);
  return $dir // '.';
}

1;

package UJPerformanceChunkSet;

use 5.010;
use strict;
use warnings;

#
# Set of Lua chunks to use for running performance tests
#

use Text::Table;

use constant {
  DEFAULT_MAX_EXP_DEGRADATION => 20,
  ALMOST_NO_DEGRADATION       => 0.5,
};

sub new {
  my $class = shift;
  my %arg   = @_;
  my $self  = {
    test_env  => $arg{test_env},
    batch     => $arg{batch},
    chunks    => $arg{chunks},

    chunk_set   => undef,
    already_run => {}, # Cache to avoid multiple undesired runs
    run_errors  => {}, # Run-time errors
    perf_errors => {}, # Unexpectedly bad performance errors
  };

  bless $self, $class;

  if ($self->{batch}) {
    $self->{chunk_set} = $self->_parse_test_batch;
  } elsif (@{ $self->{chunks} }) {
    $self->{chunk_set} = $self->_parse_test_list;
  } else {
    $self->{chunk_set} = $self->_parse_test_dir;
  }

  return $self;
}

sub is_empty      { !$_[0]->{chunk_set} || 0 == @{ $_[0]->{chunk_set} } }

sub has_no_errors {
  my $self = shift;
  return 0 == ($self->_count_errors('run_errors') + $self->_count_errors('perf_errors'));
}

#
# Parsing input
#

# Read the chunk set from the batch file, filter by --test (if passed).
# A batch files consists of lines in following format:
# file_name[.lua] thr_jit_on thr_jit_off arg1 arg2 arg3...
# Notes:
#  * thr_jit_on is max expected degradation for dev_jit_on/ref_jit_on test
#  * thr_jit_off is max expected degradation for dev_jit_off/ref_jit_off test
#  * file_name is relative to the test base if it is specified, or relative
#    to the batch file location otherwise
#  * extension is optional (for compatibility with the original PARAM_*.txt files)
#  * Lines starting with the '#' char are ignored
sub _parse_test_batch {
  my $self     = shift;
  my $base_dir = $self->{test_env}{base_dir};
  my $chunks   = $self->{chunks};

  my @chunk_set;

  open my $FH_batch, '<', $self->{batch};
  while (my $line = <$FH_batch>) {
    chomp $line;
    $line =~ s/^\s+//;
    $line =~ s/\s+$//;

    next if !length($line) || $line =~ /^#/;

    my @params = split /\s+/, $line;
    my $chunk  = $params[0];
    $chunk    .= '.lua' if $chunk !~ /\.lua$/i;
    UJPerformanceUtils::assert_file("$base_dir/$chunk");

    next if @$chunks && not grep({ $chunk eq $_ } @$chunks);

    my $thr_jit_on       = 0+ $params[1];
    my $thr_jit_off      = 0+ $params[2];
    my $thr_jit_overhead = 0+ $params[3];

    # If arguments contain stdin redirect (<file_name.txt), replace it with
    # <$TEST_BASE/file_name.txt and assert that file is available
    my @chunk_args = @params[4..$#params];
    foreach my $chunk_arg (@chunk_args) {
      if ($chunk_arg =~ /^<(.*)$/) {
        my $input_fname = $1;
        if (!length $input_fname) {
          die 'Batch file contains empty input file';
        }
        if (!-r -f "$base_dir/$input_fname") {
          die "Input file '$base_dir/$input_fname' is unavailable";
        }
        $chunk_arg =~ s|^<|<$base_dir/|;
      }
    }

    push @chunk_set, UJPerformanceChunk->new(
      test_env => $self->{test_env},
      fname    => $chunk,
      args     => \@chunk_args,
      expected => {
        'dev_jit_on/ref_jit_on'   => $thr_jit_on,
        'dev_jit_off/ref_jit_off' => $thr_jit_off,
        'dev_jit_on/dev_jit_off'  => $thr_jit_overhead,
      },
    );
  }
  close $FH_batch;

  return \@chunk_set;
}

# Read the chunk set from the base dir (--test-base + --test).
sub _parse_test_list {
  my $self     = shift;
  my $base_dir = $self->{test_env}{base_dir};
  my $chunks   = $self->{chunks};

  my @chunk_set;

  foreach my $chunk (@$chunks) {
    UJPerformanceUtils::assert_file("$base_dir/$chunk");
    push @chunk_set, UJPerformanceChunk->new(
      test_env => $self->{test_env},
      fname    => $chunk,
      args     => [],
      expected => {
        'dev_jit_on/ref_jit_on'   => DEFAULT_MAX_EXP_DEGRADATION,
        'dev_jit_off/ref_jit_off' => DEFAULT_MAX_EXP_DEGRADATION,
        'dev_jit_on/dev_jit_off'  => ALMOST_NO_DEGRADATION,
      },
    );
  }

  return \@chunk_set;
}

# Read all possible chunks from the base dir.
sub _parse_test_dir {
  my $self = shift;

  my $DH_test_dir;
  opendir $DH_test_dir, $self->{test_env}{base_dir};
  my @chunks = grep { $_ =~ /\.lua$/ } readdir $DH_test_dir;
  closedir $DH_test_dir;

  my @chucnk_set;

  foreach my $chunk (@chunks) {
    push @chucnk_set, UJPerformanceChunk->new(
      test_env => $self->{test_env},
      fname    => $chunk,
      args     => [],
      expected => {
        'dev_jit_on/ref_jit_on'   => DEFAULT_MAX_EXP_DEGRADATION,
        'dev_jit_off/ref_jit_off' => DEFAULT_MAX_EXP_DEGRADATION,
        'dev_jit_on/dev_jit_off'  => ALMOST_NO_DEGRADATION,
      },
    );
  }

  return \@chucnk_set;
}

# Run specific test with the given chunk set and print results.
sub test {
  my $self     = shift;
  my $res_type = shift;

  my ($run_mode_dev, $run_mode_ref) = split '/', $res_type;

  $self->_test($res_type, [$run_mode_dev, $run_mode_ref]);
}

sub test_simple {
  $_[0]->_test('simple', [qw/ref_jit_on dev_jit_on dev_jit_off/]);
}

# Aux method for executing a testing scenario
sub _test {
  my $self      = shift;
  my $res_type  = shift;
  my $run_modes = shift;

  my $chunk_set = $self->{chunk_set};
  my $do_deduce = $res_type ne 'simple';

  foreach my $chunk (@$chunk_set) {
    foreach my $run_mode (@$run_modes) {
      $self->_run_chunk($chunk, $run_mode);
    }

    next if !$do_deduce;

    my ($run_ok, $result_ok) = $chunk->deduce_result($res_type);
    if ($run_ok && !$result_ok) {
      $self->{perf_errors}{$res_type}++;
    }
  }

  $self->_pretty_print(UJPerformanceChunk::result_header($res_type), [
    map { $_->result_row($res_type) } sort {
      $b->result_weight($res_type) <=> $a->result_weight($res_type)
    } @$chunk_set
  ]);
}

# Print errors and some stats to stdout.
sub report_errors {
  my $self     = shift;
  my $run_only = shift;

  my $num_run_errors = $self->_count_errors('run_errors');

  if ($num_run_errors > 0) {
    my @errors = map {
      $_->has_no_run_errors? () : $_->result_row('error')
    } @{ $self->{chunk_set} };

    say 'Run-time errors';
    $self->_pretty_print(UJPerformanceChunk::result_header('error'), \@errors);
    say '';
  }

  say sprintf 'Run-time errors: %d', $num_run_errors;

  return 1 if $run_only;

  say sprintf 'Bad performance: %d', $self->_count_errors('perf_errors');
}

# Aux, count errors of the given category
sub _count_errors {
  my $self     = shift;
  my $category = shift;
  my $errors   = $self->{$category};

  my $num_errors = 0;
  foreach my $type (keys %$errors) {
    $num_errors += $errors->{$type};
  }

  return $num_errors;
}

# Aux wrapper for running a chunk in the given run mode.
sub _run_chunk {
  my $self     = shift;
  my $chunk    = shift;
  my $run_mode = shift;

  my $name        = $chunk->name;
  my $already_run = $self->{already_run};

  return if $already_run->{$run_mode}{$name};

  my $run_ok = $chunk->run_mode($run_mode);
  $already_run->{$run_mode}{$name} = 1;

  if (!$run_ok) {
    $self->{run_errors}{$run_mode}++;
  }
}

# Aux wrapper for pretty-printing results. Borrowed from
# http://use.perl.org/use.perl.org/_Ovid/journal/36762.html, don't ask.
sub _pretty_print {
  my $self   = shift;
  my $header = shift;
  my $rows   = shift;

  my @rule      = qw(- +);
  my @headers   = \'| ';
  push @headers => map { $_ => \' | ' } @$header;
  pop  @headers;
  push @headers => \' |';

  my $table = Text::Table->new(@headers);
  $table->load(@$rows);
  print
    $table->rule(@rule),
    $table->title,
    $table->rule(@rule),
    map({ $table->body($_) } 0 .. @$rows),
    $table->rule(@rule);
}

1;

package UJPerformanceChunk;

use 5.010;
use strict;
use warnings;

#
# Single Lua chunk used performance testing.
# NB! In current implementation, positive values of benchmarks denote
# performance degradation, and negative values denote gain. E.g. following
# expected benchmarks should be read as follows:
#
#  * 20%: Any performance gain or max. 20% performance degradation is ok
#  *  0%: Any performance gain or on par performance is ok
#  * -7%: Performance gain of 7% or higher is ok
#

use List::AllUtils qw(sum);
use File::Temp     qw(tempfile);

use constant {
  CHUNK_RUN_ERROR => -1,
  EXIT_SUCCESS    => 0,
  STATUS_PASS     => 'PASS',
  STATUS_WARNING  => 'OKISH',
  STATUS_FAIL     => 'FAIL',
  WEIGHT_PASS     => 0,
  WEIGHT_WARNING  => 1,
  WEIGHT_FAIL     => 2,
  FORMAT_TIMING        =>       '%.3f',
  FORMAT_GOT_DEVIATION =>   '%+-.2f%%',
  FORMAT_EXP_DEVIATION => '< %+-.2f%%',
};

sub new {
  my $class = shift;
  my %arg   = @_;

  my $self = {
    test_env => $arg{test_env},
    fname    => $arg{fname},
    args     => $arg{args}? join ' ', @{ $arg{args} } : '',
    expected => $arg{expected},

    timings  => {},
    errors   => {},
    results  => {},
    status   => {},
  };

  bless $self, $class;
  return $self;
}

sub name { $_[0]->{fname} }

sub has_no_run_errors { 0 == keys %{ $_[0]->{errors} } }

sub result_weight { $_[0]->{results}{$_[1]}{weight} }

# Runs the chunk in the given run mode. The chunk is executed nruns times.
# Returns 1 if all runs succeed, and 0 otherwise.
sub run_mode {
  my $self = shift;
  my $mode = shift;

  my $timing = $self->_run_multiple($mode);
  my $run_ok = defined($timing)? 1 : 0;

  if ($run_ok) {
    $self->{timings}{$mode} = $timing;
  }

  return $run_ok;
}

sub _deduce_status {
  my $self = shift;
  my $got  = shift;
  my $exp  = shift;

  return (STATUS_FAIL, WEIGHT_FAIL) if !defined($got);

  return (STATUS_PASS, WEIGHT_PASS) if $got <= $exp;

  # Deviation of "got" from "expected" by <= 100% issues a warning.
  # If deviation is greater, the test fails.
  my $deviation_rate = ($got - $exp) / abs($got);
  return (STATUS_WARNING, WEIGHT_WARNING) if $deviation_rate <= 1;

  return (STATUS_FAIL, WEIGHT_FAIL);
}

# Processes results of the test run and deduces the overall result.
# Returns 1 if test passed and 0 otherwise.
# "Test passed" means that *all* following conditions are true:
#  1. All runs in all modes succeeded
#  2. All timing deviations are withing acceptable limits
sub deduce_result {
  my $self     = shift;
  my $res_type = shift;

  my $timings   = $self->{timings};
  my $results   = $self->{results};
  my $expected  = $self->{expected};

  my ($run_mode_dev, $run_mode_ref) = split '/', $res_type;
  my $time_dev  = $timings->{$run_mode_dev};
  my $time_ref  = $timings->{$run_mode_ref};
  my $run_ok    = defined($time_dev) && defined($time_ref);

  my $got = $run_ok? ($time_dev / $time_ref - 1) * 100 : undef;
  my $exp = $expected->{$res_type};
  my ($status, $weight) = $self->_deduce_status($got, $exp);

  $results->{$res_type} = {
    got      => $got,
    expected => $exp,
    status   => $status,
    weight   => $weight,
  };

  return (
    $run_ok, # is run ok?
    defined($got) && $status ne STATUS_FAIL, # is perf ok?
  );
}

#
# Methods for outputting Text::Table-compatible results
#

#static
sub result_header {
  my $res_type = shift;

  if ($res_type eq 'simple') {
    return ['Test', 'Ref (JIT on)', 'Dev (JIT on)', 'Dev (JIT off)', 'Status'];
  } elsif ($res_type eq 'error') {
    return ['Test', 'Mode', 'Bad Run #', 'Core', 'Message'];
  } else {
    return ['Test', 'Ref', 'Dev', 'Got', 'Expected', 'Status'];
  }
}

sub result_row {
  my $self     = shift;
  my $res_type = shift;

  if ($res_type eq 'simple') {
    return $self->_result_row_simple;
  } elsif ($res_type eq 'error') {
    return $self->_result_rows_error;
  } else {
    return $self->_result_row($res_type);
  }
}

# Returns result row for testing in the default mode (with asserting timing)
sub _result_row {
  my $self     = shift;
  my $res_type = shift;
  my $timings  = $self->{timings};
  my $result   = $self->{results}{$res_type};

  my ($run_mode_dev, $run_mode_ref) = split '/', $res_type;

  return [
    $self->{fname},
    _format_number(FORMAT_TIMING, $timings->{$run_mode_ref}),
    _format_number(FORMAT_TIMING, $timings->{$run_mode_dev}),
    _format_number(FORMAT_GOT_DEVIATION, $result->{got}),
    _format_number(FORMAT_EXP_DEVIATION, $result->{expected}),
    $result->{status},
  ];
}

# Returns result row for testing in the simple mode (without asserting timing)
sub _result_row_simple {
  my $self    = shift;
  my $timings = $self->{timings};

  return [
    $self->{fname},
    _format_number(FORMAT_TIMING, $timings->{ref_jit_on}),
    _format_number(FORMAT_TIMING, $timings->{dev_jit_on}),
    _format_number(FORMAT_TIMING, $timings->{dev_jit_off}),
    $self->has_no_run_errors? STATUS_PASS : STATUS_FAIL,
  ];
}

# Returns error description rows for all run modes
sub _result_rows_error {
  my $self   = shift;

  my $errors    = $self->{errors};
  my @run_modes = sort keys %$errors;

  my @errors;
  foreach my $run_mode (@run_modes) {
    my $err = $errors->{$run_mode};
    my $res = $err->{err_res};
    my $msg = ((grep {
      /(?:assertion failed!)|(?:Assertion.+?failed\.)|(?:PANIC: )/;
    } @{ $res->{stderr} })[0]) // $res->{stderr}[0] // '<undef>';
    chomp $msg;

    push @errors, [
      $self->{fname},
      $run_mode,
      $err->{bad_run},
      $res->{exit}{coredump}? 'Yes' : 'No',
      $msg,
    ];
  }

  return @errors;
}

# Performs a multiple-run of a single chunk in the given mode.
# If at least one error occurs, sets per-mode error context and returns undef.
# Otherwise returns average timing.
sub _run_multiple {
  my $self = shift;
  my $mode = shift;

  my @timings;

  for my $i (1 .. $self->{test_env}{nruns}) {
    my $res = $self->_run_chunk($mode);

    if (!$res->{exit}{success}) {
      $self->{errors}{$mode} = {
        bad_run => $i,
        err_res => $res,
      };
      return;
    }

    push @timings, $res->{timing};
  }

  my $mean_timing = _mean(@timings);

  return $mean_timing;
}

# Runs a chunk in the given mode exactly once. Measures execution time with
# time(1) and return execution context, which contains timing, stdout and
# stderr as well as extra information about chunk's exit code.
sub _run_chunk {
  my $self = shift;
  my $mode = shift;
  my $env  = $self->{test_env};

  my ($interp, $jit_flag) = $self->_parse_mode($mode);

  my $stdout_fname = _create_tmp_file('stdout');
  my $stderr_fname = _create_tmp_file('stderr');

  my $chunk   = "$env->{base_dir}/$self->{fname}";
  my $command = sprintf '%s %s "%s" %s "%s" %s 1>%s 2>%s',
    '/usr/bin/time',
    '-f %U',
    $interp,
    $jit_flag,
    $chunk,
    $self->{args},
    $stdout_fname,
    $stderr_fname,
  ;
  `$env->{lua_path} $command`;

  my $stdout = _slurp_tmp_file($stdout_fname);
  my $stderr = _slurp_tmp_file($stderr_fname);

  unlink $stdout_fname, $stderr_fname;

  my $exit   = {
    status   => $?,    # See `perldoc perlvar` for differences between
    code     => undef, # $? (status) vs. "$? >> 8" (code)
    signal   => undef,
    coredump => undef,
    success  => 0,
  };

  if (defined $exit->{status}) {
    if ($exit->{status} == CHUNK_RUN_ERROR) {
      warn "Lower-level error (probably not what you want): $!";
      $exit->{code}     = $exit->{status};
      $exit->{signal}   = undef;
      $exit->{coredump} = undef;
    } else {
      $exit->{code} = $exit->{status} >> 8;
      if ($exit->{code} != EXIT_SUCCESS) {
        $exit->{signal}   = $exit->{code} & 127;
        $exit->{coredump} = $exit->{code} & 128? 1 : 0;
      } else {
        $exit->{success}  = 1;
        $exit->{coredump} = 0;
      }
    }
  }

  my $timing = $stderr->[-1];
  if (defined $timing) {
    chomp $timing;
    $timing = 0+ $timing;
  } else {
    # Oops... Something really unexpected here,
    # most probably the chunk was not even run.
    $exit->{status} = CHUNK_RUN_ERROR;
    $exit->{code}   = CHUNK_RUN_ERROR;
  }

  return {
    exit   => $exit,
    stdout => $stdout,
    stderr => $stderr,
    timing => $timing,
  };
}

sub _parse_mode {
  my $self = shift;
  my $mode = shift;
  my $env  = $self->{test_env};

  my $interp   = $mode =~ /ref/   ? $env->{interp_ref} : $env->{interp_dev};
  my $jit_flag = $mode =~ /jit_on/? ' -jon -O4' : ' -joff ';

  return $interp, $jit_flag;
}

#
# Aux routines
#

sub _mean { return @_ ? List::AllUtils::sum(@_)/@_ : 0; }

sub _create_tmp_file {
  my $infix = shift;
  my ($FH_tmp, $tmp_fname) = File::Temp::tempfile(
    sprintf('ujit_perftest_%s_XXXXX', $infix),
    TMPDIR => 1,
    UNLINK => 0,
  );
  close $FH_tmp;
  return $tmp_fname;
}

sub _slurp_tmp_file {
  my $fname = shift;
  open my $FH, '<', $fname;
  my @lines = <$FH>;
  close $FH;
  unlink $FH;
  return \@lines;
}

sub _format_number {
  my $format = shift;
  my $number = shift;
  return (defined $number)? sprintf($format, $number) : 'ERROR';
}

1;
