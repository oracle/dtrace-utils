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
export LC_COLLATE="C"

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

ZAPTHESE=

# run_with_timeout TIMEOUT CMD [ARGS ...]
#
# Run command CMD with argument ARGS, with the specified timeout.
# Return the exitcode of the command, or 126 if a timeout occurred.
#
# Note: this function depends on CHLD traps. If other subprocesses may
# exit during the invocation of this command, you may need to run it in
# a subshell.

run_with_timeout()
{
    local timeout=$1
    local cmd=$2
    local pid
    local sleepid
    local exited
    local old_ZAPTHESE

    # We must turn 'monitor' on here to ensure that the CHLD trap fires,
    # but should leave it off at all other times to prevent annoying screen
    # output.
    set -o monitor

    # We use two background jobs, one sleeping for the timeout interval, one
    # executing the desired command.  When either of these jobs exit, this trap
    # kicks in and kills off the other job.  It does a check with ps(1) to guard
    # against PID recycling causing the wrong process to be killed if the other
    # job exits at just the wrong instant.  The check for pid being set allows us
    # to avoid one ps(1) call in the common case of no timeout.
    trap 'trap - CHLD; set +o monitor; if [[ "$(ps -p $sleepid -o ppid=)" -eq $BASHPID ]]; then kill -9 $sleepid >/dev/null 2>&1; exited=1; elif [[ -n $pid ]] && [[ "$(ps -p $pid -o ppid=)" -eq $BASHPID ]]; then kill $pid >/dev/null 2>&1; exited=; fi' CHLD
    shift 2

    sleep $timeout &
    sleepid=$!
    old_ZAPTHESE="$ZAPTHESE"
    ZAPTHESE="$ZAPTHESE $sleepid"

    $cmd "$@" &
    pid=$!
    ZAPTHESE="$ZAPTHESE $pid"

    wait $pid >/dev/null 2>&1
    local exitcode=$?
    pid=

    wait $sleepid >/dev/null 2>&1

    set +o monitor
    ZAPTHESE="$old_ZAPTHESE"

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

    # Force the $_pid to expand to itself.

    _pid='$_pid'

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
 --[no-]execute: Execute probes with associated triggers.
 --[no-]capture-expected: Record results of tests that exit successfully
                          or with a timeout, but have no expected results,
                          as the expected results.
 --help: This message.

If one or more TESTs is provided, they must be the name of .d files existing
under the test/ directory.  If none is provided, all available tests are run.
EOF
exit 0
}

# Parameters.

CAPTURE_EXPECTED=
OVERWRITE_RESULTS=
NOEXEC=

ONLY_TESTS=
TESTS=
TIMEOUT=11

ERRORS=

while [[ $# -gt 0 ]]; do
    case $1 in
        --capture-expected) CAPTURE_EXPECTED=t;;
        --no-capture-expected) CAPTURE_EXPECTED=;;
        --execute) NOEXEC=;;
        --no-execute) NOEXEC=t;;
        --timeout=*) TIMEOUT="$(echo $1 | cut -d= -f2-)";;
        --help|--*) usage;;
        *) ONLY_TESTS=t
           OVERWRITE_RESULTS=t
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

# Delete this directory, and kill requested processes, at shutdown.
trap 'rm -rf ${tmpdir}; if [[ -n $ZAPTHESE ]]; then kill -9 $ZAPTHESE; fi; exit' INT QUIT TERM SEGV EXIT

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
# message.  If XFAIL is unset, XFAILMSG is ignored.  If FAILMSG
# would be printed but is unset, $testmsg is used instead.

fail()
{
    local xfail="$1"
    local xfailmsg="$2"

    shift 2
    local failmsg="$(echo ''"$@" | sed 's,^ *,,; s, $,,;')"

    if [[ -z $failmsg ]]; then
        failmsg="$testmsg"
    fi

    if [[ -n "$xfail" ]] && [[ -n "$xfailmsg" ]]; then
        out "XFAIL: $@ ($xfailmsg).\n"
    elif [[ -n "$xfail" ]]; then
        out "XFAIL: $@.\n"
    elif [[ -n $failmsg ]]; then
        ERRORS=t
        out "FAIL: $failmsg.\n"
    else
        ERRORS=t
        out "FAIL.\n"
    fi
}

# pass XFAIL MSG
#
# Report a test pass, possibly unexpected.  The output format is subtly
# different from failures, to emphasise that the message is less important
# than the fact of passing.  If MSG would be printed but is unset, $testmsg
# is used instead.

pass()
{
    if [[ -n "$1" ]]; then
        out "X"
    fi

    shift
    local msg="$(echo ''"$@" | sed 's,^ *,,; s, $,,;')"

    if [[ -z $msg ]]; then
        msg="$testmsg"
    fi

    if [[ $# -gt 0 ]] && [[ -n "$msg" ]]; then
        out "PASS ($msg).\n"
    else
        out "PASS.\n"
    fi
}

# postprocess POSTPROCESSOR OUTPUT FINAL
#
# Run postprocessing over some test OUTPUT, yielding a file named
# FINAL.  Possibly run the POSTPROCESSOR first.

postprocess()
{
    local postprocessor=$1
    local output=$2
    local final=$3
    local retval=0

    cp -f $output $tmpdir/pp.out

    # Postprocess the output, if need be.
    if [[ -x $postprocessor ]]; then
        if $postprocessor < $output > $tmpdir/output.post; then
            mv -f $tmpdir/output.post $tmpdir/pp.out
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

# More than one dtrace tree -> run tests for all dtraces, and verify identical
# intermediate code is produced by each dtrace.

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
        lcov --zerocounters --directory $name --quiet
        lcov --capture --base-directory . --directory $name --initial \
             --quiet -o $logdir/coverage/initial.lcov
    fi
done

if [[ -n $KERNEL_BUILD_DIR ]] && [[ -d $KERNEL_BUILD_DIR ]] &&
   [[ -d /sys/kernel/debug/gcov/$KERNEL_BUILD_DIR/kernel/dtrace ]]; then
        rm -rf $KERNEL_BUILD_DIR/coverage
        mkdir -p $KERNEL_BUILD_DIR/coverage
        lcov --zerocounters --quiet
        lcov --capture --base-directory $KERNEL_BUILD_DIR --initial \
             --quiet -o $KERNEL_BUILD_DIR/coverage/initial.lcov
fi

# Export some variables so triggers can get at them.
export _test _trigger_pid 

# Loop over each test in turn, or the specified subset if test names were passed
# on the command line.

# Tests are .d files, representing D programs passed to dtrace.

for dt in $dtrace; do

    if [[ -n $regression ]]; then
        out "\nTest of $dtrace:\n"
    fi

    for _test in $(if [[ $ONLY_TESTS ]]; then
                      echo $TESTS | sed 's,\.r$,\.d,g; s,\.r ,.d ,g';
                   else
                      find test -name "*.d";
                   fi); do

        base=${_test%.d}
        timeout="$TIMEOUT"

        # Hidden files and editor backup files are not tests.

        if [[ $_test =~ /\. ]] || [[ $test =~ ~$ ]]; then
            continue
        fi

        if [[ ! -e $_test ]]; then
            out "$_test: Not found.\n"
            continue
        fi

        # Various options can be set in the test .d script, usually embedded in
        # comments.
        #
        # @@runtest-opts: A set of options to be added to the default set
        #                 (normally -S -e -s, where the -S and -e may not
        #                 necessarily be provided.) Subjected to shell
        #                 expansion.
        #
        # @@timeout: The timeout to use for this test.  Overrides the --timeout
        #            parameter.
        #
        # @@skip: If true, the test is silently skipped.
        #
        # @@xfail: A single line containing a reason for this test's expected
        #          failure.  (If the test passes unexpectedly, this message is
        #          not printed.) See '.x'.
        #
        # @@trigger: A single line containing the name of a program in
        #            test/triggers which is executed after dtrace is started.
        #            When this program exits, dtrace will be killed.  If the
        #            timeout expires, both programs are killed.  Arguments may be
        #            provided as normal, and are subjected to shell expansion.
        #            For the sake of reproducible testcases, only tests with a
        #            trigger are ever executed against the kernel.  Triggers
        #            should not emit anything to stdout or stderr, as their
        #            output is not currently trapped and it will spoil the
        #            screen display.  See '.trigger'.

        # Various other files can exist with the same basename as the test.  Many
        # of these serve the same purpose as embedded comments: both exist to
        # permit identical D scripts to be symlinked together while still
        # permitting e.g. different triggering programs to be used.
        #
        # The full list of suffxes is as follows:
        #
        # .d: The D program itself.  Mandatory (the only mandatory file).
        #
        # .r: Expected results, after postprocessing.  If not present,
        #     no expected-result comparison is done (the results are
        #     merely dumped into the logfile, and the test is deemed to
        #     pass as long as dtrace exits with a zero exitcode or
        #     times out).  If any output is expected on standard error, it
        #     should be appended to this file, after a single line reading
        #     only "-- @@stderr --".
        #
        # .r.p: Expected-results postprocessor: a filter run before various
        #       hardwired pointer-value-replacement preprocessors which transform
        #       all 0x[a-f]* strings into the string {ptr} and edit out
        #       CPU and probe IDs.
        #       Receives one argument, the name of the testcase.
        #
        # .p: D script preprocessor: a filter run over the D script before
        #     passing to dtrace.  Unlike dtrace's -D options, this can vary
        #     based on the name of the test.
        #
        # .x: If executable, a program which when executed returns 0 if the program
        #     is not expected to fail and nonzero if a failure is expected (in which
        #     case it can emit to standard output a one-line message describing the
        #     reason for failure).  This permits tests to inspect their environment
        #     and determine whether failure is expected.  See 'xfail'.  If both .x
        #     and xfail exist, only the .x is respected.
        #
        # .t: If executable, serves the same purpose as the 'trigger'
        #     option above.  If both .trigger and trigger exist, only the
        #     .trigger is respected.

        # Set a variable usable in tests.

        _exit="${NOEXEC:+-e}"

        # Optionally skip this test.

        if exist_flags skip $_test; then
            continue
        fi

        # Optionally preprocess the D script.

        if [[ -x $base.p ]]; then
            preprocess $base.p $_test "$tmpdir/$(basename $_test)"
            _test="$tmpdir/$(basename $_test)"
        fi

        # Per-test timeout.

        if exist_flags timeout $_test; then
            timeout="$(extract_flags timeout $_test)"
        fi

        # Note if this is expected to fail.
        xfail=
        xfailmsg=
        if [[ -x $base.x ]]; then
            # xfail program.  Run and capture its output.
            xfailmsg="$($base.x)"
            if [[ $? -ne 0 ]]; then
                xfail=t
            fi
        elif exist_flags xfail $_test; then
            xfail=t
            xfailmsg="$(extract_flags xfail)"
        fi

        # Check for a trigger.

        trigger=
        if exist_flags trigger $_test; then
            trigger="$(extract_flags trigger $_test t)"
            if [[ ! -x test/triggers/$trigger ]]; then
                trigger=
            else
                trigger=test/triggers/$trigger
            fi
        fi

        if [[ -x $base.t ]]; then
            trigger=$base.t
        fi

        # Default and substitute in flags.

        dt_flags="$_exit -t -s $_test"

        if exist_flags runtest-opts $_test; then
             opts="$(extract_flags runtest-opts $_test t)"

            # The flags go after the dt_flags above.
            if [[ -n $opts ]] && [[ -z $override ]]; then
                dt_flags="$dt_flags $opts"
            fi
        fi

        # No trigger.  Run dtrace, with a timeout, without permitting
        # execution, recording the output and exitcode into temporary files.

        failed=
        this_noexec=$NOEXEC
        out "$_test: "
        testmsg=

        if [[ -z $trigger ]]; then
            run_with_timeout $timeout $dt $dt_flags -e > $tmpdir/test.out 2> $tmpdir/test.err
            exitcode=$?
            this_noexec=t
        else
            # A trigger.  Run dtrace with timeout, and permit execution.  First,
            # run the trigger with a 1s delay before invocation, record
            # its PID, and note it in the dt_flags if $_pid is there.
            #
            # We have to run each of these in separate subprocesses to avoid
            # the SIGCHLD from the sleep 1's death leaking into run_with_timeout
            # and confusing it. (This happens even if disowned.)

            log "Running trigger $trigger\n"
            ( sleep 1; exec $trigger; ) &
            _pid=$!
            disown %-
            ZAPTHESE="$_pid"

            if [[ $dt_flags =~ \$_pid ]]; then
                dt_flags="$(echo ''"$dt_flags" | sed 's,\$_pid[^a-zA-Z],'$_pid',g; s,\$_pid$,'$_pid',g')"
            fi

            log "Running $dt $dt_flags\n"
            ( run_with_timeout $timeout $dt $dt_flags > $tmpdir/test.out 2> $tmpdir/test.err
              echo $? > $tmpdir/dtrace.exit; )
            exitcode="$(cat $tmpdir/dtrace.exit)"

            # If the trigger is still running, kill it, and wait for it, to
            # quiesce the background-process-kill noise the shell would
            # otherwise emit.
            if [[ "$(ps -p $_pid -o ppid=)" -eq $BASHPID ]]; then
                kill $_pid >/dev/null 2>&1
            fi
            ZAPTHESE=
            unset _pid
        fi

        if [[ -n $this_noexec ]]; then
            testmsg="no execution"
        fi

        if [[ -s $tmpdir/test.err ]]; then
            echo "-- @@stderr --" >> $tmpdir/test.out
            cat $tmpdir/test.err >> $tmpdir/test.out
        fi
        rm -f $tmpdir/test.err

        if ! postprocess $base.r.p $tmpdir/test.out $tmpdir/test.out; then
            testmsg="results postprocessor failed with exitcode $?"
        fi

        # Note if we will certainly capture results.

        capturing=
        if [[ -n $CAPTURE_EXPECTED ]] && [[ -n $OVERWRITE_RESULTS ]]; then
            capturing="results captured"
        fi

        # Compare results, if available, and log the diff.

        if [[ -e $base.r ]] && ! diff -u $base.r $tmpdir/test.out >/dev/null; then
            fail "$xfail" "$xfailmsg" "expected results differ${capturing:+, $capturing}"
            log "$dt $dt_flags\n"

            cat $tmpdir/test.out >> $LOGFILE
            log "Diff against expected:\n"

            diff -u $base.r $tmpdir/test.out | tee -a $LOGFILE >> $SUMFILE
        else
            if [[ $exitcode != 0 ]] && [[ $exitcode != 126 ]]; then

                # Some sort of exitcode error.  Assume that errors in the
                # range 129 -- 193 (a common value of SIGRTMAX) are signal
                # exits.

                if [[ $exitcode -lt 129 ]] || [[ $exitcode -gt 193 ]]; then
                    fail "$xfail" "$xfailmsg" "nonzero exitcode ($exitcode)${capturing:+, $capturing}"
                else
                    fail "$xfail" "$xfailmsg" "hit by signal $((exitcode - 128))${capturing:+, $capturing}"
                fi

                log "$dt $dt_flags\n"
            else

                # Success!
                # If results don't already exist and we in capture mode, then
                # capture them even if forcible capturing is off.

                if [[ -n $CAPTURE_EXPECTED ]] && [[ ! -e $base.r ]]; then
                    capturing="results captured"
                fi

                pass "$xfail" "$xfailmsg" "$capturing"
                log "$dt $dt_flags\n"
            fi

            if [[ ! -e $base.r ]]; then
                # No expected results? Log and summarize the lot.
                tee -a < $tmpdir/test.out $LOGFILE >> $SUMFILE
            else
                # Results as expected: log them.

                cat $tmpdir/test.out >> $LOGFILE
            fi
        fi

        # If capturing results is requested, capture them now.

        if [[ -n $capturing ]]; then
            cp -f $tmpdir/test.out $base.r
        fi

        log "\n"

        if [[ $regression ]]; then
            # If regtesting, we run a second time, with intermediate results
            # displayed, and output redirected to a per-test, per-dtrace
            # temporary file. We always ban execution during regtesting:
            # our real interest is whether intermediate results have changed.

            dtest_dir="$tmpdir/regtest/$(basename $base)"
            reglog="$dtest_dir/$(echo $_test | sed 's,/,-,g').log"
            mkdir -p $dtest_dir
            echo "$dt_flags -e" > $reglog
            echo >> $reglog
            $dt "-S $dt_flags -e" 2>&1 > $tmpdir/regtest.out
            postprocess $base.r.p $tmpdir/regtest.out $tmpdir/regtest.out
            cat $tmpdir/regtest.out >> $reglog

            if [[ -z $first_dtest_dir ]]; then
                first_dtest_dir=$dtest_dir
            fi
        fi
    done
done

if [[ -n $regression ]]; then
    # Regtest comparison.  Skip one directory and diff all the others
    # against it.

    out "\n\nRegression test comparison.\n"

    for name in $tmpdir/regtest/*; do
        [[ $name = $first_dtest_dir ]] && continue

        if ! diff -urN $first_dtest_dir $name | tee -a $LOGFILE $SUMFILE; then
            out "Regression test comparison failed."
        fi
    done
fi

# Test coverage.
for name in build-*; do
    if [[ -n "$(echo $name/*.gcda)" ]]; then
        out "Coverage info for $name:\n"
        lcov --capture --base-directory . --directory $name \
             --quiet -o $logdir/coverage/runtest.lcov
        lcov --add-tracefile $logdir/coverage/initial.lcov \
             --add-tracefile $logdir/coverage/runtest.lcov \
             --quiet -o $logdir/coverage/coverage.lcov
        genhtml --frames --show-details -o $logdir/coverage \
                --title "DTrace userspace coverage" \
                --highlight --legend $logdir/coverage/coverage.lcov | \
            awk 'BEGIN { quiet=1; } { if (!quiet) { print ($0); } } /^Overall coverage rate:$/ { quiet=0; }' | \
            tee -a $LOGFILE $SUMFILE
    fi
done

if [[ -n $KERNEL_BUILD_DIR ]] && [[ -d $KERNEL_BUILD_DIR ]] &&
       [[ -d /sys/kernel/debug/gcov/$KERNEL_BUILD_DIR/kernel/dtrace ]]; then
    out "Coverage info for kernel:\n"

    lcov --capture --base-directory $KERNEL_BUILD_DIR \
         --quiet -o $KERNEL_BUILD_DIR/coverage/coverage.lcov
    lcov --add-tracefile $KERNEL_BUILD_DIR/coverage/initial.lcov \
         --add-tracefile $KERNEL_BUILD_DIR/coverage/coverage.lcov \
         --quiet -o $KERNEL_BUILD_DIR/coverage/coverage.lcov

    genhtml --frames --show-details -o $KERNEL_BUILD_DIR/coverage \
            --title "DTrace kernel coverage" --highlight --legend \
            $KERNEL_BUILD_DIR/coverage/coverage.lcov | \
        awk 'BEGIN { quiet=1; } { if (!quiet) { print ($0); } } /^Overall coverage rate:$/ { quiet=0; }' | \
        tee -a $LOGFILE $SUMFILE
fi

if [[ -n $ERRORS ]]; then
    exit 1
fi

exit 0
