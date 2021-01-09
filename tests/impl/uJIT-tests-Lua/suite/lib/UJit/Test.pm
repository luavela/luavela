package UJit::Test;

# Mini-framework for testing uJIT. Allows to run arbitrary Lua chunks from files
# or strings, specify command-line options, inspect exit code and the contents
# of stdout and stderr.
# Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use base 'Exporter';

use Cwd qw/getcwd abs_path/;
use Test::More;
use File::Spec;
use File::Temp;
use FindBin qw/$Script/;

use constant {
    DUMMY_CHUNK_NAME => 'chunk',
    DUMMY_INLINE_NAME => '[inline chunk]',
    DEFAULT_UJIT_NAME => 'ujit',
    DEFAULT_PROFILE_MOCKER_NAME => 'ujit-mock-profile',
    DEFAULT_PROFILE_PARSER_NAME => 'ujit-parse-profile',
    DEFAULT_MEMPROF_PARSER_NAME => 'ujit-parse-memprof',
    FIND_DEPTH_LEVEL => 10,
    PROJECT_ROOT_ANCHOR => 'src',
};

#
# Defining and exporting tool names:
#

our %UJIT_TOOLS;
our @UJIT_TOOLS_SYM;

BEGIN {
    %UJIT_TOOLS = (
        PROF_MOCKER    => 'mocker',
        PROF_PARSER    => 'parser',
        MEMPROF_PARSER => 'memprof',
    );
    @UJIT_TOOLS_SYM = (keys %UJIT_TOOLS);
}

# Exportable constants for tools:
use constant \%UJIT_TOOLS;

our @EXPORT_OK = (@UJIT_TOOLS_SYM);
our %EXPORT_TAGS = (tools => [@UJIT_TOOLS_SYM]);

sub _ensure_dir
{
    my ($self, $dir) = @_;

    if (!-d $dir) {
        BAIL_OUT("Unable to mkdir '$dir': $!") unless mkdir $dir;
    }

    return $dir; # ...for convenience
}

sub _ensure_bin
{
    my ($self, $bin) = @_;

    $bin = Cwd::abs_path($bin);
    BAIL_OUT("$bin is not readable") unless -f -r $bin;
    BAIL_OUT("$bin is not executable") unless -x $bin;

    return $bin; # ...for convenience
}

sub _tool_bin
{
    my ($self, $tool) = @_;

    return File::Spec->catfile(
        $self->{tools_dir},
        $self->{tools}{$tool}{name},
    );
}

# Tries to guess locations of binaries that were built in-source.
sub _get_bin_dir
{
   my $bin_dir = PROJECT_ROOT_ANCHOR;
   for (my $level = 0; $level < FIND_DEPTH_LEVEL; $level++) {
       $bin_dir = "../$bin_dir";
       return $bin_dir if -d $bin_dir;
   }
   BAIL_OUT(sprintf("Failed out to find '%s' dir after %d attempts",
        PROJECT_ROOT_ANCHOR,
        FIND_DEPTH_LEVEL
   ));
}

sub new
{
    my $class = shift;
    my %arg   = @_;

    my $self = {
        ujit_name  => $arg{ujit_name}  // $ENV{UJIT_BIN} // DEFAULT_UJIT_NAME,
        ujit_dir   => $arg{ujit_dir }  // $ENV{UJIT_DIR} // _get_bin_dir(),
        chunks_dir => $arg{chunks_dir} // '',
        tools_dir  => $arg{tools_dir}  // '',

        # Options prepended by default to all runs of uJIT:
        ujit_options => $ENV{UJIT_OPTIONS} // '',

        tools => {
            mocker => {
                name => $arg{mocker_name} // DEFAULT_PROFILE_MOCKER_NAME,
            },
            parser => {
                name => $arg{parser_name} // DEFAULT_PROFILE_PARSER_NAME,
            },
            memprof => {
                name => $arg{memprof_name} // DEFAULT_MEMPROF_PARSER_NAME,
            }
        },

        # Create a special object for situations when a test wants
        # to emulate writing to a non-writable file
        # NB: created in '/tmp' as removed afterwards.
        bad_file   => File::Temp->newdir(
            'ujit-tmp-dir-XXXXXXXXXX',
            TMPDIR  => 1,
            CLEANUP => 1,
        ),

        last_run => {
            name          => '',   # Run name which is used to name artifacts dir
            chunk         => '',
            inline        =>  0,
            stdout        => [],
            stderr        => [],
            exit_status   => undef, # Exit status of the last run as returned by the "system" function
            exit_code     => undef, # Exit code as returned by the uJIT binary
            exit_signal   => undef, # In case of failure: Exit signal number
            exit_coredump => undef, # In case of failure: Flag, whether or not coredump was generated
        },
        chunk_num => 0, # Numbering of chunks; numbers are used for tests which don't run lua chunks as '.lua' files but raw strings via 'loadstring'.
    };

    if (!defined $self->{bad_file}) {
        BAIL_OUT('Suspicious env: bad_file could not be created, please check');
    }

    foreach my $param (qw/ujit_dir chunks_dir tools_dir/) {
        if (length($self->{$param}) && $self->{$param} !~ m|/$|) {
            $self->{$param} .= '/';
        }
    }

    bless $self, $class;

    $self->{rundir} = $self->_ensure_dir(
        File::Spec->catfile(Cwd::getcwd(), 'tests-run')
    );

    $self->{ujit_cmd} = $self->_ensure_bin(
        File::Spec->catfile($self->{ujit_dir}, $self->{ujit_name})
    );

    if ($self->{tools_dir}) {
        foreach my $tool (values %UJIT_TOOLS) {
            $self->_ensure_bin($self->_tool_bin($tool));
        }
    }

    return $self;
}

sub DESTROY
{
    done_testing();
}

# Returns the name in the file system hierarchy that is
# guaranteed to represent a non-writable file.
sub non_writable_fname { return $_[0]->{bad_file}->dirname }

# Return absolute path to a dir within the artifact dir.
sub dir
{
    my ($self, $dir) = @_;

    return $self->_ensure_dir(
        File::Spec->catfile($self->{rundir}, $dir)
    );
}

#
# $tester->run($chunk_fname, opt1 => val1, opt2 => val2, ...);
#
# Runs a Lua chunck from file $chunk_fname and collects output info
# (stdout, stderr, exit code, coredump etc.) for subsequent assertions.
#
# Supported options:
#   * jit      => BOOLEAN: Turns JIT compilation on/off. Default value is 1 (JIT compilation ON)
#   * inline   => BOOLEAN: First argument is a Lua code, but not a chunk name
#   * args     => STRING:  Raw string of any command line arguments recognized by uJIT
#   * lua_args => STRING:  Raw string of whitespace-delimited arguments to the Lua chunk
#   * env      => HASHREF: environment variables: { KEY1 => "VAL1", KEY2 => "VAL2", ... }
sub run
{
    my $self  = shift;
    my $chunk = shift;
    my %opt   = @_;

    my $lr = $self->{last_run};
    $lr->{chunk}  = $chunk;
    $lr->{inline} = $opt{inline} // 0;

    my $chunk_full;

    if (!$opt{inline}) {
        $chunk_full = "$self->{chunks_dir}$chunk";
    } else {
        # NB! Better escaping will be added on demand.
        $chunk_full = sprintf "-e '%s'", $chunk;
    }

    # Adjust LUA_PATH if custom ujit_dir and/or chunks_dir is used.
    # This is done so that modules shipped with uJIT (e.g. jit.*)
    # and aux modules for chunks themselves could be loaded without problems.
    my $lua_path = join ';', map {
        $self->{$_} ? File::Spec->catfile($self->{$_}, '?.lua') : ()
    } qw/ujit_dir chunks_dir/;

    # NB! Env variable names and their values are not validated/escaped,
    # please fix on demand.
    my %env_vars = (
        lua_path   => $lua_path ? qq/LUA_PATH="$lua_path"/ : '',
        other_vars => $opt{env}
            ? join(' ', map { "$_=$opt{env}{$_}" } keys %{$opt{env}})
            : ''
        ,
    );

    my $jit_flag = ($opt{jit} // 1) ? '' : '-j off ';
    my %cmd_args = (
        ujit_args  => $jit_flag . ($opt{args} // ''),
        chunk      => $chunk_full,
        chunk_args => $opt{lua_args} // '',
    );

    $self->_run_cmd({
       name => $self->_get_chunk_name($opt{inline}, $chunk),
       cmd => "$self->{ujit_cmd} $self->{ujit_options}",
       env_vars => [ @env_vars{ qw/lua_path other_vars/ } ],
       cmd_args => [ @cmd_args{ qw/ujit_args chunk chunk_args/ } ],
    });

    return $self;
}

#
# $tester->run_tool($tool, $test_name, opt1 => val1, opt2 => val2, ...);
#
# Runs a tool and collects output info
# (stdout, stderr, exit code, coredump etc.) for subsequent assertions.
#
# Supported options:
#   * args      => STRING:   String of any command line arguments recognized by uJIT
#   * out_dir   => STRING:   Directory for storying output
#   * valgrind  => HASHREF:  Options for running the tool with valgrind
sub run_tool
{
    my $self  = shift;
    my $tool  = shift;
    my $tname = shift;
    my %opt   = @_;

    if (!exists $self->{tools}{$tool}) {
        my $known_tools = join ', ', sort keys %{$self->{tools}};
        BAIL_OUT("Unknown tool $tool. Known tools are: $known_tools");
    }

    my $run_name = sprintf('tool_%s_%s', $tool, $tname);
    my $out_dir  = $opt{out_dir} // "$self->{rundir}/$run_name";
    my $tool_cmd = $self->_tool_bin($tool);
    my $valgrind_log = "$out_dir/valgrind.log";

    if ($opt{valgrind}) {
        BAIL_OUT('valgrind is not in $PATH') unless qx/which valgrind/;
        $tool_cmd = "valgrind --log-file=$valgrind_log $tool_cmd";
    }

    $self->_run_cmd({
       name     => $run_name,
       cmd      => $tool_cmd,
       env_vars => [],
       cmd_args => [$opt{args}],
       out_dir  => $out_dir,
    });

    if ($opt{valgrind}) {
        $self->_file_matches($valgrind_log, qr/ERROR SUMMARY: 0 errors/);
        $self->_file_matches($valgrind_log,
            qr/((no leaks are possible)|(definitely lost: 0 .+indirectly lost: 0 .+possibly lost: 0 .+reachable: 0))/s
        ) if $opt{valgrind}{strict};
    }

    return $self;
}

sub exit_ok
{
    my $self  = shift;
    my $tname = shift;
    return $self->_exit_check($tname, $self->{last_run}{exit_status} == 0);
}

sub exit_not_ok
{
    my $self  = shift;
    my $tname = shift;
    return $self->_exit_check($tname, $self->{last_run}{exit_status} != 0);
}

sub exit_with_coredump
{
    my $self  = shift;
    my $tname = shift;
    return $self->_exit_check($tname, $self->{last_run}{exit_coredump} == 1);
}

sub exit_without_coredump
{
    my $self  = shift;
    my $tname = shift;
    return $self->_exit_check($tname, $self->{last_run}{exit_coredump} == 0);
}

sub stdout_has    { (shift)->_stdstream_check('stdout', 'has', @_); }
sub stderr_has    { (shift)->_stdstream_check('stderr', 'has', @_); }
sub stdout_has_no { (shift)->_stdstream_check('stdout', 'no',  @_); }
sub stderr_has_no { (shift)->_stdstream_check('stderr', 'no',  @_); }
sub stdout_eq     { (shift)->_stdstream_check('stdout', 'eq',  @_); }
sub stderr_eq     { (shift)->_stdstream_check('stderr', 'eq',  @_); }
sub stdout_ne     { (shift)->_stdstream_check('stdout', 'ne',  @_); }
sub stderr_ne     { (shift)->_stdstream_check('stderr', 'ne',  @_); }

sub stdout_matches { (shift)->_stdstream_check('stdout', 'matches',  @_); }
sub stderr_matches { (shift)->_stdstream_check('stderr', 'matches',  @_); }
sub stdout_matches_no { (shift)->_stdstream_check('stdout', 'matches_no',  @_); }
sub stderr_matches_no { (shift)->_stdstream_check('stderr', 'matches_no',  @_); }

sub _run_cmd
{
    my $self = shift;
    my $args = shift;

    # NB! Artifacts from the previous run are overwritten.
    my $run_name = $args->{name};
    my $dir_path = $self->_ensure_dir(
        $args->{out_dir} // File::Spec->catfile($self->{rundir}, $run_name)
    );
    my $stdout = File::Spec->catfile($dir_path, 'ujit_test_stdout');
    my $stderr = File::Spec->catfile($dir_path, 'ujit_test_stderr');

    my $full_cmd = join (' ',
        @{$args->{env_vars}},
        $args->{cmd},
        @{$args->{cmd_args}},
        "1>$stdout",
        "2>$stderr",
    );

    system $full_cmd;

    my $exit_status = $?;

    BAIL_OUT("Unable to system $full_cmd") unless defined $exit_status;

    my $lr = $self->{last_run};

    $lr->{name}        = $run_name;
    $lr->{exit_status} = $exit_status;
    $lr->{stdout}      = $self->_slurp_tmp_file($stdout);
    $lr->{stderr}      = $self->_slurp_tmp_file($stderr);

    if ($exit_status == -1) {
        warn "Lower-level error (probably not what you want): $!";
        $lr->{exit_code}     = $exit_status;
        $lr->{exit_signal}   = undef;
        $lr->{exit_coredump} = undef;
    } else {
        $lr->{exit_code} = $exit_status >> 8;
        if ($lr->{exit_code} & 127) {
            $lr->{exit_signal}   = $lr->{exit_code} & 127;
            $lr->{exit_coredump} = $lr->{exit_code} & 128 ? 1 : 0;
        } else {
            $lr->{exit_signal}   = undef;
            $lr->{exit_coredump} = 0;
        }
    }
}

sub _get_chunk_name
{
    my $self = shift;
    my $inline = shift;
    my $chunk = shift;

    my $script_name = $FindBin::Script;
    $script_name =~ s/\.t$//i;

    my $test_name;

    if ($inline) {
        $test_name = join('_', $script_name, DUMMY_CHUNK_NAME, $self->{chunk_num});
        $self->{chunk_num} += 1;
    } else {
        ($test_name = join('_', $script_name, $chunk)) =~ s|/|_|g;
    }
    return $test_name;
}

sub _exit_check
{
    my $self  = shift;
    my $tname = shift;
    my $exp   = shift;

    my $caller = (caller(1))[3];
    if (!ok($exp, $self->_neat_tname($tname, $caller))) {
        my $lr     = $self->{last_run};
        my $stdout = join '', @{$lr->{stdout}};
        my $stderr = join '', @{$lr->{stderr}};
        diag(sprintf "stdout contents:$/%s$/$/stderr contents:$/%s",
            $stdout, $stderr,
        );
    }
    return $self;
}

sub _stdstream_check
{
    my $self   = shift;
    my $stream = shift;
    my $mode   = shift;
    my $exp    = shift; # Expected contents (will be escaped if needed)
    my $tname  = shift;

    my $stream_lines = $self->{last_run}{$stream};
    my $stream_cont  = join '', @$stream_lines; # Full contents of the stream

    my $assertion = 0;
    if ($mode =~ /^(has|no)$/) {
        my $exp_esc   = ref($exp) eq 'Regexp'? $exp : qr/\Q$exp\E/;
        my $num_found = grep { $_ =~ $exp_esc } @$stream_lines;
        $assertion    = $mode eq 'has'? $num_found != 0 : $num_found == 0;
    } elsif ($mode =~ /^(eq|ne)$/) {
        $assertion = $mode eq 'eq'? $stream_cont eq $exp : $stream_cont ne $exp;
    } elsif ($mode =~ /^(matches|matches_no)$/) {
        my $exp_esc = ref($exp) eq 'Regexp'? $exp : qr/\Q$exp\E/s;
        my $matches = $stream_cont =~ $exp_esc;
        $assertion  = $mode eq 'matches'? $matches : !$matches;
    } else {
        BAIL_OUT('_stdstream_check: invalid mode (internal)');
    }

    my $caller = (caller(1))[3];
    ok($assertion, $self->_neat_tname($tname, $caller))
        or diag(sprintf "%s contents:$/%s", $stream, $stream_cont);

    return $self;
}

sub _file_matches
{
    my ($self, $fname, $regexp) = @_;
    my $matches = 0;

    open(my $FH, '<', $fname) or BAIL_OUT("Unable to open $fname: $!");
    while (my $line = <$FH>) {
        if ($line =~ $regexp) {
            $matches = 1;
            last;
        }
    }
    close $FH;

    ok($matches, "File $fname matches $regexp");

    return $self;
}

sub _slurp_tmp_file
{
    my $self  = shift;
    my $fname = shift;
    open(my $FH, '<', $fname) or BAIL_OUT("Unable to open $fname: $!");
    my @lines = <$FH>;
    close $FH;
    return \@lines;
}

sub _neat_tname
{
    my $self   = shift;
    my $tname  = shift;
    my $caller = shift // (caller(1))[3];
    my $lr     = $self->{last_run};

    my $chunk_name = $lr->{inline}? DUMMY_INLINE_NAME : "$self->{chunks_dir}$lr->{chunk}";
    return sprintf '%s: %s', $chunk_name, $tname // $caller;
}

1;
