#!/usr/bin/perl
#
# Tests for table.* and ujit.table.*.
# Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

use 5.010;
use warnings;
use strict;
use lib './lib';

use UJit::Test;

use constant {
    IRDUMP_OPTION => '-p-'
};

sub _iterate_tests {
    my $run = shift;
    my $check = shift;
    my $expressions = shift;

    foreach my $expr (@{$expressions}) {
        $run->$check($expr);
    }
}

sub _run_tests_group {
    my $extlib_func_name = shift;
    my $tester = shift;
    my @tests = @_;

    foreach my $test (@tests) {
        my %asserts = %{$test->{asserts}};

        my $run_result = $tester->run($test->{file},
                                      args => IRDUMP_OPTION,
                                      lua_args => $extlib_func_name);

        while(my($check, $expr_list) = each %asserts) {
            if (scalar @{$expr_list}) {
                _iterate_tests($run_result, $check, $expr_list);
            }
            else {
                $run_result->$check();
            }
        }
    }
}

sub run_tests {
    my $tester = shift;
    my $tests = shift;

    while (my ($extlib_func_name, $tests_group) = each %{$tests}) {
        _run_tests_group($extlib_func_name, $tester, @{$tests_group});
    }
}

my $tester = UJit::Test->new(
    chunks_dir => './chunks/table',
);

my @tarray = (
    'table_creation_tests.lua',
    'mm/nargs.lua',
);

foreach my $t (@tarray) {
    $tester->run($t)->exit_ok();
}

# ujit.table extensions comments:
#
# 'shallowcopy':
#
# 'IR_TDUP' now used for recording 'shallowcopy': its argument is not constant
# thus jit optimizations that had implied constness are no longer applicable.
# Special tests for 'shallowcopy' recording verify that they are not applied.

# ujit.table extensions library recording checks.

my @funcs = ('keys', 'shallowcopy', 'values', 'toset', 'size' );

# Tests for all the 'ujit.table' functions.

my $common_recording_tests = [{
    file => 'recording.lua',
    asserts => {
        exit_ok => [],
        stdout_has => [qr/TRACE .+? stop -> loop/],
        stdout_has_no => [
            qr/TRACE.+?abort.+?NYI: FastFunc ujit\.table.*/,
            qr/TRACE.+?abort.+?asynchronous abort/],
    }}, {
    file => 'throw_on_recording.lua',
    asserts => {
        exit_not_ok => [],
        stdout_has => [qr/TRACE.+?abort.+?asynchronous abort/],
        stderr_has => [qr/table expected, got number/],
        stdout_has_no => [qr/TRACE .+? stop -> loop/],
    }}
];

run_tests($tester, { map {$_ => $common_recording_tests} @funcs});

my $special_recording_tests = {
    keys => [{
        file => 'recording.lua',
        asserts => {
            exit_ok => [],
            stdout_has => [
                qr/tab CALLL.+?lj_tab_keys/,
                qr/call.+?->lj_tab_keys/,
                # gc step is recorded for allocating built-in function
                qr/->lj_gc_step_jit/
            ]
        }
    }],
    values => [{
        file => 'recording.lua',
        asserts => {
            exit_ok => [],
            stdout_has => [
                qr/tab CALLL.+?lj_tab_values/,
                qr/call.+?->lj_tab_values/,
                # gc step is recorded for allocating built-in function
                qr/->lj_gc_step_jit/
            ]
        }
    }],
    toset => [{
        file => 'recording.lua',
        asserts => {
            exit_ok => [],
            stdout_has => [
                qr/tab CALLL.+?lj_tab_toset/,
                qr/call.+?->lj_tab_toset/,
                qr/->lj_gc_step_jit/
            ]
        }
    }],
    shallowcopy => [{
        file => 'recording.lua',
        asserts => {
            exit_ok => [],
            stdout_has => [
                'TDUP'
            ]
        }
    }, {
        file => 'recording/asize_shallowcopy.lua',
        asserts => {
            exit_ok => [],
            stdout_has => [
                # should NOT be folded for non-const tab (can be substituted in
                # place for const tab: array part size)
                qr/int FLOAD  .*  tab.asize/,
            ]
        }
    }, {
        file => 'recording/hload_shallowcopy.lua',
        asserts => {
            exit_ok => [],
            stdout_has => [
                # should NOT be folded for non-const tab (can be substituted in
                # place for const tab: hash preallocated size)
                qr/FLOAD  .*  tab.hmask/,
                # should not be folded (i.e. substituted with actual value)
                qr/>  str HLOAD/,
            ]
        }
    }, {
        file => 'recording/href_store_shallowcopy.lua',
        asserts => {
            exit_ok => [],
            stdout_has => [
               # should NOT be folded for non-const tab (can be substituted in
               # place for const tab: hash node value offset)
               qr/p32 HREF   .*  .*/,
            ]
        }
    }, {
        file => 'recording/tbar_shallowcopy.lua',
        asserts => {
            exit_ok => [],
            stdout_has_no => [
                # TBAR which succeeds HSTORE should be removed by folding for
                # invariant loop part;
                # table created via TDUP for shallowcopy is not different from
                # other tables created via TDUP/TNEW
                qr/ nil TBAR/s,
            ]
        }
    }, {
        file => 'recording/sink_shallowcopy.lua',
        asserts => {
            exit_ok => [],
            stdout_has_no => [
                # TDUP emitted for shallowcopy should not be sinked as currently
                # its sinking would damage restore from snapshot
                qr/.* \{sink\}\+ tab TDUP .*/s,
            ]
        }
   }]
};

run_tests($tester, $special_recording_tests);

# table.concat compilation checks
$tester->run('concat/concat.lua', args => '-p-')
  ->exit_ok()
  ->stdout_has_no(qr/TRACE.+?abort.+?/)
  ->stdout_has(qr/TRACE.+?stop -> loop/)
  ->stdout_has(qr/CALLL.+?lj_tab_concat/)
  ->stdout_has(q/lj_gc_step_jit/);

$tester->run('concat/concat_error.lua', args => '-p-')
  ->stdout_has_no(qr/TRACE.+?abort.+?/)
  ->stdout_has(qr/TRACE.+?stop -> loop/)
  ->stdout_has(qr/CALLL.+?lj_tab_concat/)
  ->stdout_has(q/lj_gc_step_jit/)
  ->exit_not_ok()
  ->exit_without_coredump()
  ->stderr_has(q/invalid value (nil) at index 2/);

$tester->run('concat/concat_throw_on_recording1.lua', args => '-p-')
  ->exit_not_ok()
  ->exit_without_coredump()
  ->stdout_has(qr/TRACE.+?asynchronous abort/)
  ->stderr_has(q/bad argument #1 to 'concat' (table expected/);

$tester->run('concat/concat_throw_on_recording2.lua', args => '-p-')
  ->exit_not_ok()
  ->exit_without_coredump()
  ->stdout_has(qr/TRACE.+?asynchronous abort/)
  ->stderr_has(q/invalid value (nil) at index 2/);

# Non-trivial tests for ujit.table.toset
$tester->run('toset/literals.lua')->exit_ok;
$tester->run('toset/mutated.lua')->exit_ok;

# table.size tests
$tester->run('size/error_handling.lua')
  ->exit_ok()
  ->exit_without_coredump();

$tester->run('size/size.lua')
  ->exit_ok()
  ->exit_without_coredump();

# table.size recording check
$tester->run('size/size_recording.lua', args => '-p-')
  ->exit_ok()
  ->stdout_has_no(qr/TRACE.+?abort.+?/)
  ->stdout_has(qr/TRACE.+?stop -> loop/)
  ->stdout_has(qr/CALLL.+?lj_tab_size/);

$tester->run('size/throw_on_recording.lua', args => '-p-')
  ->exit_not_ok()
  ->exit_without_coredump()
  ->stdout_has(qr/TRACE.+?asynchronous abort/)
  ->stderr_has(q/bad argument #1 to 'size' (table expected/);

$tester->run('debug.lua')
  ->exit_ok();

$tester->run('rindex/interface.lua')
  ->exit_ok();

$tester->run('rindex/recording.lua', args => '-p-')
  ->exit_ok()
  ->stdout_has_no(qr/TRACE.+?abort.+?/)
  ->stdout_has(qr/\bTVARG\b/)
  ->stdout_has(qr/\bTVARGF\b/)
  ->stdout_has(qr/\bTVLOAD\b/)
  ->stdout_has(qr/CALLL.+?lj_tab_rawrindex_jit/)
;

$tester->run('rindex/no-recording.lua', args => '-p-')
  ->exit_ok()
  ->stdout_has('NYI: unsupported variant of FastFunc')
  ->stdout_has('NYI: bytecode')
  ->stdout_has('guard would always fail')
  ->stdout_has('asynchronous abort')
;

$tester->run('rindex/tailcall.lua', args => '-Ohotloop=3 -Ohotexit=2 -p-')
  ->exit_ok()
  ->stdout_matches(qr/.+TRACE.3.IR.+\s
                      0024.rax.+p64.CALLL.+lj_tab_rawrindex_jit.+\(0023\)\s
                      0025.+>.+p64.NE.+0024.+\[NULL\]\s
                      0026.+rax.+>.+str.TVLOAD.0024\s
                      0027.+>.+p32.RETF\s
                      .+SNAP.+\#1\s
                      .+TRACE.3.mcode.+/xs)
  ->stdout_matches(qr/.+TRACE.6.IR.+\s
                      0024.rax.+p64.CALLL.+lj_tab_rawrindex_jit.+\(0023\)\s
                      0025.+>.+p64.NE.+0024.+\[NULL\]\s
                      0026.+>.+tab.TVLOAD.0024\s
                      0027.+>.+p32.RETF\s
                      .+SNAP.+\#1\s
                      .+TRACE.6.mcode.+/xs)
  ->stdout_matches(qr/.+TRACE.7.IR.+\s
                      0025.rax.+p64.CALLL.+lj_tab_rawrindex_jit.+\(0024\)\s
                      0026.+>.+p64.NE.+0025.+\[NULL\]\s
                      0027.+>.+fal.TVLOAD.0025\s
                      .+LOOP.+\s
                      0034.rax.+p64.CALLL.+lj_tab_rawrindex_jit.+\(0033\)\s
                      0035.+>.+p64.NE.+0034.+\[NULL\]\s
                      0036.+>.+fal.TVLOAD.0034\s
                      .+TRACE.7.mcode.+/xs)
;

exit;
