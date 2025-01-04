#!/bin/bash
#
# Wrapping runner script for all benchmarks.
# Not integrated into uJIT build/test chain, should be run
# and evaluated manually or via uJIT's CI.
# Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

UJIT_TEST_DIR=`pwd`
UJIT_BIN=$UJIT_TEST_DIR/../src/ujit

#
# Runner options
#

DEFAULT_MULTIRUN=5
MULTIRUN=$DEFAULT_MULTIRUN

function show_help()
{
    cat <<EOF
SYNOPSIS

$0 [OPTIONS]

DESCRIPTION

This runner starts benchmarks for uJIT.
Supported options are (subset of options provided by perftest.pl):

--multirun NUMBER [optional] Number of runs for each chunk (default: $DEFAULT_MULTIRUN)
--help            [optional] Show this message and exit

EOF
}

while :; do
    case $1 in
        --help)
            show_help
            exit
            ;;
        --multirun)
            if [ -n "$2" ]; then
                MULTIRUN=$(($2 + 0))
                shift
            else
                printf 'ERROR: --multirun requires a number.\n' >&2
                exit 1
            fi
            ;;
        -?*)
            printf 'WARNING: Unknown option (ignored): %s\n' "$1" >&2
            ;;
        *)
            break
    esac

    shift
done

#
# Run performance tester:
#

EXIT_SUCCESS=0
EXIT_FAILURE=1
EXIT_STATUS=$EXIT_SUCCESS

function run_api_benchmark()
{
    ./run_api_benchmarks.sh

    if [[ $? -ne $EXIT_SUCCESS ]]; then
        EXIT_STATUS=$EXIT_FAILURE
    fi
}

function run_perftest_benchmark()
{
    # Currently we do not test against any external reference interpreter
    # (neither LuaJIT nor previous stable uJIT tag), hence --skip-* options.
    perl perftest.pl --skip-jit-on --skip-jit-off --multirun $MULTIRUN \
        --interp $UJIT_BIN --batch $1/PARAM_x86_ujit.txt

    if [[ $? -ne $EXIT_SUCCESS ]]; then
        EXIT_STATUS=$EXIT_FAILURE
    fi
}

run_api_benchmark
run_perftest_benchmark $UJIT_TEST_DIR/impl/LuaJIT-tests/suite/bench
run_perftest_benchmark $UJIT_TEST_DIR/iponweb/perf/chunks

#
# Deduce overall result and exit:
#

if [[ $EXIT_STATUS -eq $EXIT_SUCCESS ]]; then
    echo "Overall: PASS"
else
    echo "Overall: FAIL"
fi

exit $EXIT_STATUS
