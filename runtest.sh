#!/bin/bash
#
# runtest.sh -- A dtrace tester and regression tester.
#
#               Runs a bunch of .d scripts and tests their results.
#               If more than dtrace build directory is available,
#               runs them each in turn, comparing their output
#               and generated intermediate representation.
#

# Sanitize the shell and get configuration info.

shopt -s nullglob
unset CDPATH
unset POSIXLY_CORRECT  # Interferes with 'wait'

[[ -f ./runtest.conf ]] && . ./runtest.conf

# get_dir_name
#
# Pick a unique temporary directory name to stick things in.
# Returns results via `echo'; must be called via substitution.
#
# Respect TMPDIR if set, as POSIX requires.

get_dir_name()
{
    local tmpname;                  # A subdir name being tested for uniqueness

    false;                            # Starting state - reset $?
    while [[ $? -ne 0 ]]; do
        tmpname=${TMPDIR:-/tmp}/runtest.${RANDOM}
        mkdir $tmpname                # Test-and-set it
    done

    echo $tmpname                     # Name is unique, return it
}

# find_next_numeric_dir DIR
#
# Create and return the first empty numerically-named directory under DIR.

find_next_numeric_dir()
{
    local i;

    mkdir -p $1
    for ((i=0; ; i++)); do
        if [[ ! -e $1/$i ]]; then
            mkdir -p $1/$i
            rm -f $1/current
            ln -s $i $1/current
            echo $1/$i
            return
        fi
    done
}

# call_with_timeout TIMEOUT CMD [ARGS ...]
#
# Call command CMD with argument ARGS, with the specified timeout.
# Return the exitcode of the command, or 126 if a timeout occurred.

call_with_timeout()
{
    local timeout=$1
    local cmd=$2
    local pid
    local sleepid
    local exited

    # We must turn 'monitor' on here to ensure that the CHLD trap fires,
    # but should leave it off at all other times to prevent annoying screen
    # output.
    set -o monitor

    # We use two background jobs, one sleeping for the timeout interval, one
    # executing the desired command. When either of these jobs exit, this trap
    # kicks in and kills off the other job. It does a check with ps(1) to guard
    # against PID recycling causing the wrong process to be killed if the other
    # job exits at just the wrong instant. The check for pid being set allows us
    # to avoid one ps(1) call in the common case of no timeout.
    trap 'trap - CHLD; set +o monitor; if [[ "$(ps -p $sleepid -o ppid=)" -eq $$ ]]; then kill -9 $sleepid >/dev/null 2>&1; exited=1; elif [[ -n $pid ]] && [[ "$(ps -p $pid -o ppid=)" -eq $$ ]]; then kill -9 $pid >/dev/null 2>&1; exited=; fi' CHLD
    shift 2

    sleep $timeout &
    sleepid=$!

    $cmd "$@" &
    pid=$!

    wait $pid >/dev/null 2>&1
    local exitcode=$?
    pid=

    wait $sleepid >/dev/null 2>&1

    if [[ -n $exited ]]; then
        return $exitcode
    else
        return 126
    fi
}

# exist_flags NAME FILE
#
# Return 0 iff a set of runtest flags named NAME exists in FILE.
#
# The flags have the form @@NAME.

exist_flags()
{
    grep -Eq "@@"$1 $2
}

# extract_flags NAME FILE EXPAND
#
# Extract the flags named NAME from FILE, and return them on stdout.
# If EXPAND is set, do a round of shell evaluation on them first.

extract_flags()
{
    local val="$(grep -E "@@"$1 $2 | sed 's,.*@@'$1' *: ,,; s,\*/$,,')"

    # This trick is because printf squashes out spaces, echo -e -e foo
    # prints 'foo', and echo -e -- "foo" prints "-- foo".

    if [[ -n $3 ]]; then
        eval echo ''"$val"
    else
        echo ''"$val"
    fi
}

usage()
{
    cat <<EOF
$0 -- testsuite engine for DTrace

Usage: runtest options [TEST ...]

Options:
 --timeout=TIME: Time out test runs after TIME seconds (default 10).
                 (Timeouts are not considered test failures: timing out
                 is normal.)
 --[no-]capture-expected: Record results of tests that exit successfully
                          or with a timeout, but have no expected results,
                          as the expected results.
 --help: This message.

If one or more TESTs is provided, they must be the name of .d files existing
undwer the test/ directory. If none is provided, all available tests are run.
EOF
exit 0
}

# Parameters.

CAPTURE_EXPECTED=

ONLY_TESTS=
TESTS=
TIMEOUT=10

while [[ $# -gt 0 ]]; do
    case $1 in
        --capture-expected) CAPTURE_EXPECTED=t;;
        --no-capture-expected) CAPTURE_EXPECTED=;;
        --timeout=*) TIMEOUT="$(echo $1 | cut -d= -f2-)";;
        --help|--*) usage;;
        *) ONLY_TESTS=t
           if [[ -f $1 ]]; then
               TESTS="$TESTS $1"
           else
               echo "Test $1 not found, ignored." >&2
           fi;;
    esac
    shift
done

# Store test results in a directory with an incrementing name.
# We arbitrarily assume that we are being run from ~: since
# this is what 'make check' does, that's a reasonable
# assumption.

cd $(dirname $0)
logdir=$(find_next_numeric_dir test/log)

LOGFILE=$logdir/runtest.log
SUMFILE=$logdir/runtest.sum

# Make a temporary directory for intermediate result storage.

typeset tmpdir=$(get_dir_name)
trap 'cd; rm -rf ${tmpdir}; exit' INT QUIT TERM SEGV EXIT

# Log and failure functions.

out()
{
    printf "$@"
    sum "$@"
}

sum()
{
    printf "$@" >> $SUMFILE
    log "$@"
}

log()
{
    printf "$@" >> $LOGFILE
}

# fail XFAIL XFAILMSG FAILMSG
#
# Report a test failure, whether expected or otherwise, with the given
# message. If XFAIL is unset, XFAILMSG is ignored.

fail()
{
    local xfail="$1"
    local xfailmsg="$2"

    shift
    if [[ -n "$xfail" ]] && [[ -n "$xfailmsg" ]]; then
        out "XFAIL: $@ ($xfailmsg).\n"
    elif [[ -n "$xfail" ]]; then
        out "XFAIL: $@.\n"
    else
        out "FAIL: $@.\n"
    fi
}

# pass XFAIL MSG
#
# Report a test pass, possibly unexpected. The output format is subtly
# different from failures, to emphasise that the message is less important
# than the fact of passing.

pass()
{
    if [[ -n "$1" ]]; then
        out "X"
    fi

    shift
    if [[ $# -gt 0 ]]; then
        out "PASS ($@).\n"
    else
        out "PASS.\n"
    fi
}

# postprocess POSTPROCESSOR OUTPUT FINAL
#
# Run postprocessing over some test OUTPUT, yielding a file named
# FINAL. Possibly run the POSTPROCESSOR first.

postprocess()
{
    local postprocessor=$1
    local output=$2
    local final=$3
    local retval=0

    cp -f $output $tmpdir/pp.out

    # Postprocess the output, if need be.
    if [[ -x $postprocessor ]]; then
        if $postprocessor < $output > $tmpdir/pp.out; then
            mv -f $tmpdir/output.post $final
        else
            retval=$?
            cp -f $output $tmpdir/pp.out
        fi
    fi

    # Transform any printed hex strings into a fixed string.
    # TODO: may need adjustment or making optional if scripts emit hex
    # values which are not continuously variable.
    
    sed 's,0x[0-9a-f][0-9a-f]*,{ptr},g' < $tmpdir/pp.out > $final
    return $retval
}

dtrace="./build-*/dtrace"

if [[ "$(eval echo $dtrace)" = './build-*/dtrace' ]]; then
	echo "No dtraces available.";
	exit 1;
fi

# Variables usable in demo flags.

# TODO: allow specification of a program to auto-launch and later
# attach to, instead of using our own shell's PID. Allow
# overriding of -e, so as to test things with execution (but
# this requires some way to force the execution to terminate:
# perhaps time-based via wait(1)?).

_pid=`echo $$`
_exit="-e"

# More than one dtrace tree -> verify identical results for each dtrace.

regression=
first_dtest_dir=
if [[ $(echo $dtrace | wc -w) -gt 1 ]]; then
    regression=t
fi

# Initialize test coverage.

for name in build-*; do
    if [[ -n "$(echo $name/*.gcno)" ]]; then
        rm -rf $logdir/coverage
        mkdir -p $logdir/coverage
        lcov --capture --base-directory . --directory $name --initial \
             --quiet -o $logdir/coverage/initial.lcov
    fi
done

if [[ -n $KERNEL_BUILD_DIR ]] && [[ -d $KERNEL_BUILD_DIR ]] &&
   [[ -d /sys/kernel/debug/gcov/$KERNEL_BUILD_DIR/kernel/dtrace ]]; then
        rm -rf $KERNEL_BUILD_DIR/coverage
        mkdir -p $KERNEL_BUILD_DIR/coverage
        lcov --capture --base-directory $KERNEL_BUILD_DIR --initial \
             --quiet -o $KERNEL_BUILD_DIR/coverage/initial.lcov
fi

# Loop over each test in turn, or the specified subset if test names were passed
# on the command line.

# Tests are .d files, representing D programs passed to dtrace.

for dt in $dtrace; do

    if [[ -n $regression ]]; then
        out "\nTest of $dtrace:\n"
    fi

    for _test in $(if [[ $ONLY_TESTS ]]; then echo $TESTS; else find test -name "*.d"; fi); do

        base=${_test%.d}
        timeout="$TIMEOUT"

        if [[ ! -e $_test ]]; then
            out "$_test: Not found."
            continue
        fi

        # Various options can be set in the test .d script, usually embedded in
        # comments.
        #
        # runtest-opts: A set of options to be added to the default set (normally -S
        #               -e -s, where the -S and -e may not necessarily be provided.)
        #
        # timeout: The timeout to use for this test. Overrides the --timeout
        #          parameter.
        #
        # skip: If true, the test is silently skipped.
        #
        # xfail: A single line containing a reason for this test's expected failure.
        #        (If the test passes unexpectedly, this message is not printed.)

        # Various other files can exist with the same basename as the test. Many
        # of these serve the same purpose as embedded comments: both exist to
        # permit identical D scripts to be symlinked together while still permitting
        # e.g. different triggering programs to be used.
        #
        # The full list of suffxes is as follows:
        #
        # .d: The D program itself. Mandatory (the only mandatory file).
        #
        # .r: Expected results, after postprocessing. If not present,
        #     no expected-result comparison is done (the results are
        #     merely dumped into the logfile, and the test is deemed to
        #     pass as long as dtrace exits with a zero exitcode or
        #     times out).
        #
        # .r.p: Expected-results postprocessor: a filter run before a hardwired
        #       pointer-value-replacement preprocessor which transforms
        #       all 0x[a-f]* strings into the string {ptr}, so this
        #       postprocessor can do things like pointer-value correlation.
        #       Receives one argument, the name of the testcase.
        #
        # .p: D script preprocessor: a filter run over the D script before
        #     passing to dtrace. Unlike dtrace's -D options, this can vary
        #     based on the name of the test.
        #
        # .x: If executable, a program which when executed returns 0 if the program
        #     is not expected to fail and nonzero if a failure is expected (in which
        #     case it can emit to standard output a one-line message describing the
        #     reason for failure). This permits tests to inspect their environment
        #     and determine whether failure is expected. See 'xfail'. If both .x
        #     and xfail exist, only the .x is respected.
        #

        # Optionally skip this test.

        if exist_flags skip $_test; then
            continue
        fi

        # Optionally preprocess the D script.

        if [[ -x $base.p ]]; then
            preprocess $base.p $_test "$tmpdir/$(basename $_test)"
            _test="$tmpdir/$(basename $_test)"
        fi

        dt_flags="$_exit -s $_test"

        # Substitute in flags.

        if exist_flags runtest_opts $_test; then
             opts="$(extract_flags runtest_opts $_test t)"

            # The flags go after the dt_flags above.
            if [[ -n $opts ]] && [[ -z $override ]]; then
                dt_flags="$dt_flags $opts"
            fi
        fi

        # Per-test timeout.

        if exist_flags timeout $_test; then
            timeout="$(extract_flags timeout $_test)"
        fi

        # Note if this is expected to fail.
        xfail=
        xfailmsg=
        if [[ -x $base.x ]]; then
            # xfail program. Run and capture its output.
            xfailmsg="$($base.x)"
            if [[ $? -eq 0 ]]; then
                xfail=t
            fi
        elif exist_flags xfail $_test; then
            xfail=t
            xfailmsg="$(extract_flags xfail)"
        fi

        # Run dtrace, with a timeout, recording the output into a
        # temporary file.

        failed=
        out "$_test: "
        call_with_timeout $timeout $dt $dt_flags > $tmpdir/test.out 2>&1

        exitcode=$?

        if ! postprocess $base.r.p $tmpdir/test.out $tmpdir/test.out; then
            out "(results postprocessor failed with exitcode $?) "
        fi

        # Compare results, if available, and log the diff.

        if [[ -e $base.r ]] && ! diff -u $base.r $tmpdir/test.out >/dev/null; then
            fail "$xfail" "$xfailmsg" "expected results differ"
            log "$dt $dt_flags\n"

            cat $tmpdir/test.out >> $LOGFILE
            log "Diff against expected:\n"

            diff -u $base.r $tmpdir/test.out | tee -a $LOGFILE >> $SUMFILE
        else
            if [[ $exitcode != 0 ]] && [[ $exitcode != 126 ]]; then

                # Some sort of exitcode error. Assume that errors in the
                # range 129 -- 193 (a common value of SIGRTMAX) are signal
                # exits.

                if [[ $exitcode -lt 129 ]] || [[ $exitcode -gt 193 ]]; then
                    fail "$xfail" "$xfailmsg" "nonzero exitcode ($exitcode)"
                else
                    fail "$xfail" "$xfailmsg" "hit by signal $((exitcode - 128))"
                fi

                log "$dt $dt_flags\n"
            else

                # Success!

                # Generate results, if requested.

                if [[ -n $CAPTURE_EXPECTED ]]; then
                    cp $tmpdir/test.out $base.r
                    pass "$xfail" "$xfailmsg" "results captured"
                else
                    pass "$xfail" "$xfailmsg"
                fi

                log "$dt $dt_flags\n"
            fi

            if [[ ! -e $base.r ]]; then
                # No expected results? Log and summarize the lot.
                tee -a < $tmpdir/test.out $LOGFILE >> $SUMFILE
            else
                # Results as expected: only log them.
                cat $tmpdir/test.out >> $LOGFILE
            fi
        fi

        log "\n"

        if [[ $regression ]]; then
            # If regtesting, we run a second time, with intermediate results
            # displayed, and output redirected to a per-test, per-dtrace
            # temporary file.

            dtest_dir="$tmpdir/regtest/$(basename $base)"
            reglog="$(echo $_test | sed 's,/,-,g').log"
            mkdir -p $dtest_dir
            echo $dt_flags > $dtest_dir/$reglog
            echo >> $dtest_dir/$reglog
            $dt -S $dt_flags 2>&1 > $tmpdir/regtest.out
            postprocess $base.r.p $tmpdir/regtest.out $tmpdir/regtest.out
            cat $tmpdir/regtest.out >> $reglog

            if [[ -z $first_dtest_dir ]]; then
                first_dtest_dir=$dtest_dir
            fi
        fi
    done
done

if [[ -n $regression ]]; then
    # Regtest comparison. Skip one directory and diff all the others
    # against it.

    out "\n\nRegression test comparison.\n"

    for name in $tmpdir/regtest/*; do
        [[ $name = $first_dtest_dir ]] && continue

        if ! diff -urN $first_dtest_dir $name | tee -a $LOGFILE | tee -a $SUMFILE; then
            out "Regression test comparison failed."
        fi
    done
fi

# Test coverage.
for name in build-*; do
    if [[ -n "$(echo $name/*.gcda)" ]]; then
        echo "Coverage info for $name:"
        lcov --capture --base-directory . --directory $name \
             --quiet -o $logdir/coverage/runtest.lcov
        lcov --add-tracefile $logdir/coverage/initial.lcov \
             --add-tracefile $logdir/coverage/runtest.lcov \
             --quiet -o $logdir/coverage/coverage.lcov
        genhtml --frames --show-details -o $logdir \
                --title "DTrace userspace coverage" \
                --highlight --legend $logdir/coverage/coverage.lcov | \
            awk 'BEGIN { quiet=1; } { if (!quiet) { print ($0); } } /^Overall coverage rate:$/ { quiet=0; }'
    fi
done

if [[ -n $KERNEL_BUILD_DIR ]] && [[ -d $KERNEL_BUILD_DIR ]] &&
       [[ -d /sys/kernel/debug/gcov/$KERNEL_BUILD_DIR/kernel/dtrace ]]; then
    echo "Coverage info for kernel:"

    lcov --capture --base-directory $KERNEL_BUILD_DIR \
         --quiet -o $KERNEL_BUILD_DIR/coverage/coverage.lcov
    lcov --add-tracefile $KERNEL_BUILD_DIR/coverage/initial.lcov \
         --add-tracefile $KERNEL_BUILD_DIR/coverage/coverage.lcov \
         --quiet -o $KERNEL_BUILD_DIR/coverage/coverage.lcov

    genhtml --frames --show-details -o $KERNEL_BUILD_DIR/coverage \
            --title "DTrace kernel coverage" --highlight --legend \
            $KERNEL_BUILD_DIR/coverage/coverage.lcov | \
        awk 'BEGIN { quiet=1; } { if (!quiet) { print ($0); } } /^Overall coverage rate:$/ { quiet=0; }'
fi
