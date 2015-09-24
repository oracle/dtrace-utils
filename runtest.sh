#!/bin/bash
#
# runtest.sh -- A dtrace tester and regression tester.
#
#               Runs a bunch of .d scripts and tests their results.
#               If more than one dtrace build directory is available,
#               runs them each in turn, comparing their output
#               and generated intermediate representation.
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License, Version 1.0 only
# (the "License").  You may not use this file except in compliance
# with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
# Copyright 2011 -- 2015 Oracle, Inc.  All rights reserved.
# Use is subject to license terms.
#
#

# Sanitize the shell and get configuration info.

shopt -s nullglob extglob
unset CDPATH
unset POSIXLY_CORRECT  # Interferes with 'wait'
export LC_COLLATE="C"

TIMEOUTSIG=KILL

arch="$(uname -m)"

[[ -f ./runtest.conf ]] && . ./runtest.conf

load_modules()
{
    # If running as root, pull in appropriate modules
    if [[ "x$(id -u)" = "x0" ]]; then
        # Use a loop to pick up the first dtrace instance if regtesting
        for dt in $dtrace; do
            # Look for our shared libraries in the right place.
            if [[ "x$test_libdir" != "xinstalled" ]]; then
                export LD_LIBRARY_PATH="$(dirname $dt)"
            fi

            DTRACE_MODULES_CONF="$(pwd)/test/modules" $dt -qn 'BEGIN { exit(0); }' >/dev/null
            unset LD_LIBRARY_PATH

            comm -13 \
                 <(lsmod | awk '{print $1}' | sort -u) \
                 <(cat test/modules | grep -v '^#' | grep -v '# unload-quietly' | sed 's,#.*$,,; s, *$,,' | sort -u) > $tmpdir/failed-modules
            if test -s $tmpdir/failed-modules; then
                echo -e "Error: cannot load all modules:" >&2
                cat $tmpdir/failed-modules >&2
                exit 1
            fi
            break
        done
    else
        echo "Warning: testing as non-root may cause a large number of unexpected failures." >&2
    fi
}

unload_modules()
{
    HIDE=$1
    # If running as root, unload all appropriate modules
    if [[ "x$(id -u)" = "x0" ]]; then
        tac ./test/modules | while read -r line; do
            name="$(echo $line | sed 's,##[a-z-]*,,g')"
            [[ -z $name ]] && continue;
            if [[ -z $HIDE ]] && echo "$line" | grep -qv '# unload-quietly'; then
                rmmod $(echo $name | sed 's,#.*$,,')
            else
                rmmod $(echo $name | sed 's,#.*$,,') 2>/dev/null
            fi
        done
    fi
}

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
    local dtpid
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
    #
    # Note: because log, sum, out and force_out invoke subprocesses, you cannot
    # call either of them while the CHLD trap is in force.
    shift 2
    log "Running $cmd $@ with timeout $timeout\n"

    trap 'trap - CHLD; set +o monitor; if [[ "$(ps -p $sleepid -o ppid=)" -eq $BASHPID ]]; then kill -$TIMEOUTSIG -$sleepid >/dev/null 2>&1; exited=1; elif [[ -n $pid ]] && [[ "$(ps -p $pid -o ppid=)" -eq $BASHPID ]]; then kill $TIMEOUTSIG -$pid >/dev/null 2>&1; exited=; fi' CHLD
    sleep $timeout &
    sleepid=$!
    old_ZAPTHESE="$ZAPTHESE"
    ZAPTHESE="$ZAPTHESE $sleepid"

    $cmd "$@" &
    pid=$!
    dtpid=$pid
    ZAPTHESE="$ZAPTHESE $pid"
    wait $pid >/dev/null 2>&1
    local exitcode=$?
    pid=

    wait $sleepid >/dev/null 2>&1

    trap - CHLD
    set +o monitor
    ZAPTHESE="$old_ZAPTHESE"

    if [[ "$(ps -p $dtpid -o ppid=)" -eq $BASHPID ]]; then
        force_out "ERROR: hanging dtrace detected. Testing terminated.\n"
        ps -p $dtpid -o args= | tee -a $LOGFILE >> $SUMFILE
        # Terminate the test run early: future runs cannot succeed,
        # and test coverage output is meaningless.
        unload_modules
        exit 1
    fi

    # Nonzero exitcode?  Wait for a second, to allow any coredump to trickle
    # out to disk.
    [[ $exitcode -ne 0 ]] && sleep 1

    if dmesg | grep -q 'Bad page'; then
        echo 'Bad page state!' >&2
        exit 1
    fi

    if [[ -n $exited ]]; then
        return $exitcode
    else
        return 126
    fi
}

# exist_options NAME FILE
#
# Return 0 iff a set of runtest options named NAME exists in FILE,
# or in the test.options file in its directory or in any parent
# up to test/.
#
# The options have the form @@NAME.

exist_options()
{
    local retval=1
    local path=$2
    local file=$2

    while [[ $retval -ne 0 ]]; do
        if [[ -e "$file" ]]; then
            grep -Eq "@@"$1 $file
            retval=$?
        fi
        path="$(dirname $path)"
        file="$path/test.options"
        if [[ -e "$path/test.$arch.options" ]]; then
            file="$path/test.$arch.options"
        fi
        # Halt at top level.
        if [[ -e $path/Makecheck ]]; then
            break
        fi
    done

    return $retval
}

# extract_options NAME FILE EXPAND
#
# Extract the options named NAME from FILE, or in the test.options file in its
# directory or any parent up to test/, and return them on stdout.
#
# If EXPAND is set, do a round of shell evaluation on them first.

extract_options()
{
    local retval=1
    local path=$2
    local file=$2

    while :; do
        # This horrible sequence of patterns catches @@foo, @@foo: bar, and @@foo */,
        # while not catching @@foo-bar or @@foo-bar: baz. */
        if [[ -f $file ]] && grep -Eq -e "@@"$1' *:' -e "@@"$1" " -e "@@"$1'$' $file; then
            local val="$(grep -E -e "@@"$1' *:' -e "@@"$1" " -e "@@"$1'$' $file | sed 's,.*@@'$1' *: ,,; s,\*/$,,; s,  *$,,')"

            # Force the $_pid to expand to itself.

            _pid='$_pid'

            # This trick is because printf squashes out spaces, echo -e -e foo
            # prints 'foo', and echo -e -- "foo" prints "-- foo".

            if [[ -n $3 ]]; then
                eval echo ''"$val"
            else
                echo ''"$val"
            fi
            return
        fi
        path="$(dirname $path)"
        file="$path/test.options"
        # Halt at top level.
        if [[ -e $path/Makefunctions ]]; then
            return
        fi
    done
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
 --testsuites=SUITES: Which testsuites (directories under test/) to run,
                      as a comma-separated list.
 --[no-]execute: Execute probes with associated triggers.
 --[no-]comparison: (Do not) compare results against expected results.
                    If result comparison is suppressed, XPASSes are
                    silently converted to PASSes (catering for expected
                    failures which are shown only via a miscomparison).
 --[no-]capture-expected: Record results of tests that exit successfully
                          or with a timeout, but have no expected results,
                          as the expected results.  (Only useful if
                          --no-comparison is not specified.)
 --[no-]baddof: Run corrupt-DOF tests.
 --[no-]use-installed: Use an installed dtrace rather than a copy in the
                       source tree.
 --load-modules-only: Trigger unloading, optional installation and loading
                      of modules, then immediately exit.
 --quiet: Only show unexpected output (FAILs and XPASSes).
 --verbose: The opposite of --quiet (and the default).
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
BADDOF=
USE_INSTALLED=
TESTSUITES="unittest internals stress demo"
VALGRIND=
COMPARISON=t
LOAD_MODULES_ONLY=

ONLY_TESTS=
TESTS=
QUIET=
TIMEOUT=11

ERRORS=

while [[ $# -gt 0 ]]; do
    case $1 in
        --load-modules-only) LOAD_MODULES_ONLY=t;;
        --capture-expected) CAPTURE_EXPECTED=t;;
        --no-capture-expected) CAPTURE_EXPECTED=;;
        --execute) NOEXEC=;;
        --no-execute) NOEXEC=t;;
        --comparison) COMPARISON=t;;
        --no-comparison) COMPARISON=;;
        --valgrind) VALGRIND=t;;
        --no-valgrind) VALGRIND=;;
        --baddof) BADDOF=;;
        --no-baddof) BADDOF=t;;
        --use-installed) USE_INSTALLED=t;;
        --no-use-installed) USE_INSTALLED=;;
        --timeout=*) TIMEOUT="$(printf -- $1 | cut -d= -f2-)";;
        --testsuites=*) TESTSUITES="$(printf -- $1 | cut -d= -f2- | tr "," " ")";;
        --quiet) QUIET=t;;
        --verbose) QUIET=;;
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

# If running as root, remember and turn off core_pattern, and set the
# coredumpsize to a biggish value.

orig_core_pattern=
orig_core_uses_pid=

if [[ "x$(id -u)" = "x0" ]]; then
    orig_core_pattern="$(cat /proc/sys/kernel/core_pattern)"
    orig_core_uses_pid="$(cat /proc/sys/kernel/core_uses_pid)"
    echo core > /proc/sys/kernel/core_pattern
    echo 0 > /proc/sys/kernel/core_uses_pid
    ulimit -c 1000000
fi

# Make a temporary directory for intermediate result storage.
# Make a binary directory within it, for wrapper scripts.

export tmpdir="$(get_dir_name)"
mkdir $tmpdir/bin
export PATH=$tmpdir/bin:$PATH

# At shutdown, delete this directory, kill requested process groups, and restore
# core_pattern.
trap 'rm -rf ${tmpdir}; if [[ -n $ZAPTHESE ]]; then kill -9 -$ZAPTHESE; fi; if [[ -z $orig_core_pattern ]]; then echo $orig_core_pattern > /proc/sys/kernel/core_pattern; echo $orig_core_uses_pid > /proc/sys/kernel/core_uses_pid; fi; exit' INT QUIT TERM SEGV EXIT

# Log and failure functions.

out()
{
    if [[ -z $QUIET ]]; then
        printf "%s" "$*" | sed 's,%,%%,g' | xargs -0n 1 printf
    fi
    sum "$@"
}

force_out_only()
{
    printf "%s" "$*" | sed 's,%,%%,g' | xargs -0n 1 printf
}

force_out()
{
    force_out_only "$@"
    sum "$@"
}

sum()
{
    printf "%s" "$*" | sed 's,%,%%,g' | xargs -0n 1 printf >> $SUMFILE
    log "$@"
}

log()
{
    printf "%s" "$*" | sed 's,%,%%,g' | xargs -0n 1 printf >> $LOGFILE
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

    # In quiet mode, we may need to print out the test name too.
    [[ -n $QUIET ]] && [[ -z $xfail ]] && force_out_only "$_test: "

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
        force_out "FAIL: $failmsg.\n"
    else
        ERRORS=t
        force_out "FAIL.\n"
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
    local xpass="$1"
    local xout=out

    # In quiet mode, we may need to print out the test name, and will
    # want to print out the PASS message if and only if this was an
    # XPASS.

    if [[ -n $xpass ]] && [[ -n $COMPARISON ]]; then
        [[ -n $QUIET ]] && force_out_only "$_test: "
        force_out "X"
        xout=force_out
    fi

    shift
    local msg="$(echo ''"$@" | sed 's,^ *,,; s, $,,; s,^# *,,')"

    if [[ -z $msg ]]; then
        msg="$testmsg"
    fi

    if [[ $# -gt 0 ]] && [[ -n "$msg" ]]; then
        $xout "PASS ($msg).\n"
    else
        $xout "PASS.\n"
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

    # Transform any printed hex strings into a fixed string, excepting
    # only valgrind errors.
    # TODO: may need adjustment or making optional if scripts emit hex
    # values which are not continuously variable.

    sed '/^==[0-9][0-9]*== /!s,0x[0-9a-f][0-9a-f]*,{ptr},g' < $tmpdir/pp.out > $final

    return $retval
}

if [[ -z $USE_INSTALLED ]]; then
    dtrace="$(pwd)/build*/dtrace"
    test_libdir="$(pwd)/build/dlibs"
    test_incflags="-Iuts/common -Ilibdtrace-ctf/include -DARCH_$arch"

    if [[ -z $(eval echo $dtrace) ]]; then
    	echo "No dtraces available." >&2
    	exit 1
    fi
else
    dtrace="/usr/sbin/dtrace"
    test_libdir="installed"
    test_incflags="-DARCH_$arch"

    if [[ ! -x $dtrace ]]; then
        echo "$dtrace not available." >&2
        exit 1
    fi
fi

# Flip a few env vars to tell dtrace to produce reproducible output.
export _DTRACE_TESTING=t
export LANGUAGE=C

# Figure out if the preprocessor supports -fno-diagnostics-show-option: if it
# does, add a bunch of options designed to make GCC output look like it used
# to.
# Regardless, turn off display of column numbers, if possible: they vary
# between GCC releases.

test_cppflags=
if ! /usr/bin/cpp -x c -fno-diagnostics-show-caret - /dev/null < /dev/null 2>&1 | \
	grep -q 'unrecognized command line option'; then
    export DTRACE_OPT_CPPARGS="-fno-diagnostics-show-caret -fdiagnostics-color=never -fno-diagnostics-show-option -fno-show-column"
elif ! /usr/bin/cpp -x c -fno-show-column - /dev/null < /dev/null 2>&1 | \
	grep -q 'unrecognized command line option'; then
    export DTRACE_OPT_CPPARGS="-fno-show-column"
fi

# More than one dtrace tree -> run tests for all dtraces, and verify identical
# intermediate code is produced by each dtrace.

regression=
first_dtest_dir=
if [[ $(echo $dtrace | wc -w) -gt 1 ]]; then
    regression=t
fi

# If not testing an installed dtrace, we must look for our system .d files
# in the right place.  (load_dtrace_modules is also found here.)
if [[ "x$test_libdir" != "xinstalled" ]]; then
    export DTRACE_OPT_SYSLIBDIR="$test_libdir"
fi

# Unload all modules before initializing test coverage: then ask dtrace to load
# them again afterwards, to acquire initialization and shutdown coverage.
# If we are in load-only mode, load and exit here.)
unload_modules hide

if [[ -n $LOAD_MODULES_ONLY ]]; then
    load_modules
    exit $?
fi

# Initialize test coverage.

if [[ -z $BADDOF ]]; then
    for name in build*; do
        if [[ -n "$(echo $name/*.gcno)" ]]; then
            rm -rf $logdir/coverage
            mkdir -p $logdir/coverage
            lcov --zerocounters --directory $name --quiet
            lcov --capture --base-directory . --directory $name --initial \
                 --quiet -o $logdir/coverage/initial.lcov 2>/dev/null
        fi
    done

    if [[ -n $KERNEL_BUILD_DIR ]] && [[ -d $KERNEL_BUILD_DIR ]] &&
       [[ -d /sys/kernel/debug/gcov/$KERNEL_BUILD_DIR/kernel/dtrace ]]; then
            rm -rf $KERNEL_BUILD_DIR/coverage
            mkdir -p $KERNEL_BUILD_DIR/coverage
            lcov --zerocounters --quiet
            lcov --capture --base-directory $KERNEL_BUILD_DIR --initial \
                 --quiet -o $KERNEL_BUILD_DIR/coverage/initial.lcov 2>/dev/null
    fi
fi

load_modules

if [[ -n $BADDOF ]]; then
    # Run DOF-corruption tests instead.

    test/utils/badioctl > dev/null 2> $tmpdir/badioctl.err &
    declare ioctlpid=$!

    ZAPTHESE="$ioctlpid"
    for _test in $(if [[ $ONLY_TESTS ]]; then
                      echo $TESTS | sed 's,\.r$,\.d,g; s,\.r ,.d ,g'
                   else
                      for name in $TESTSUITES; do
                          find test/$name -name "*.d" | sort -u
                      done
                   fi); do

        if ! run_with_timeout $TIMEOUT test/utils/baddof $_test > /dev/null 2> $tmpdir/baddof.err; then
            out "$_test: FAIL (bad DOF)"
        fi
        if [[ -s $tmpdir/baddof.err ]]; then
            sum "baddof stderr:"
            tee -a < $tmpdir/baddof.err $LOGFILE >> $SUMFILE
        fi
    done

    if [[ "$(ps -p $ioctlpid -o ppid=)" -eq $BASHPID ]]; then
        kill -$TIMEOUTSIG $ioctlpid >/dev/null 2>&1
    fi
    if [[ -s $tmpdir/badioctl.err ]]; then
        sum "badioctl stderr:"
        tee -a < $tmpdir/badioctl.err $LOGFILE >> $SUMFILE
    fi
    ZAPTHESE=
    exit 0
fi


# Export some variables so triggers and .sh scripts can get at them.
export _test _pid dt_flags

# Arrange to do (relatively expensive) mutex debugging.
export DTRACE_OPT_DEBUGASSERT="mutexes"

# If running DTrace inside valgrind, make sure valgrind works, and multiply
# the timeout by ten.
if [[ -n $VALGRIND ]]; then
    if ! valgrind -q /bin/true >/dev/null 2>&1; then
        echo "valgrind does not work: cannot run in --valgrind mode." >&2
        exit 1
    fi
    TIMEOUT=$((TIMEOUT * 10))
fi

# Loop over each test in turn, or the specified subset if test names were passed
# on the command line.

# Tests are .d files, representing D programs passed to dtrace, or .sh files,
# representing shell scripts which invoke dtrace and analyze the results.

for dt in $dtrace; do

    if [[ -n $regression ]]; then
        force_out "\nTest of $dt:\n"
    fi

    # Look for our shared libraries in the right place.
    if [[ "x$test_libdir" != "xinstalled" ]]; then
        export LD_LIBRARY_PATH="$(dirname $dt)"
    fi

    # Log versions.
    $dt -vV | tee -a $LOGFILE >> $SUMFILE
    uname -a | tee -a $LOGFILE >> $SUMFILE
    modinfo dtrace | grep 'version.*:' | tee -a $LOGFILE >> $SUMFILE
    if [[ -f .git-version ]]; then
	sum "testsuite version-control ID: $(cat .git-version)\n\n"
    elif [[ -f .git-archive-version ]]; then
	sum "Testsuite version-control ID: $(cat .git-archive-version)\n\n"
    fi

    # If we are running valgrind, invoke dtrace via a wrapper.

    if [[ -n $VALGRIND ]]; then
	cat >> $tmpdir/bin/$(basename $dt) <<-EOT
	#!/bin/bash
	# Wrap DTrace calls inside valgrind.
	valgrind -q $dt "\$@"
	EOT
        dt="$tmpdir/bin/$(basename $dt)"
	chmod a+x $tmpdir/bin/$(basename $dt)
    fi

    for _test in $(if [[ $ONLY_TESTS ]]; then
                      echo $TESTS | sed 's,\.r$,\.d,g; s,\.r ,.d ,g'
                   else
                      for name in $TESTSUITES; do
                          find test/$name \( -name "*.d" -o -name "*.sh" \) | sort -u
                      done
                   fi); do

        base=${_test%.d}
        base=${base%.sh}
        testonly="$(basename $_test)"
        timeout="$TIMEOUT"

        # Hidden files and editor backup files are not tests.

        if [[ $_test =~ /\. ]] || [[ $test =~ ~$ ]]; then
            continue
        fi

        if [[ ! -e $_test ]]; then
            force_out "$_test: Not found.\n"
            continue
        fi

        # Various options can be set in the test .d script, usually embedded in
        # comments.  In addition, any options listed in a file test.options in 
        # any directory from test/ on down will automatically be imposed on
        # any tests in that directory tree which do not themselves impose a
        # value for that option.  If a file named test.$arch.options exists at
        # a given level, it is consulted instead of test.options at that level.
        #
        # @@runtest-opts: A set of options to be added to the default set
        #                 (normally -S -e -s, where the -S and -e may not
        #                 necessarily be provided.) Subjected to shell
        #                 expansion.
        #
        # @@timeout: The timeout to use for this test.  Overrides the --timeout
        #            parameter.
        #
        # @@skip: If true, the test is skipped.
        #
        # @@xfail: A single line containing a reason for this test's expected
        #          failure.  (If the test passes unexpectedly, this message is
        #          not printed.) See '.x'.
        #
        # @@no-xfail: If present, this means that a test is *not* expected to
        #             fail, even though a test.options in a directory above
        #             this test says otherwise.
        #
        # @@trigger: A single line containing the name of a program in
        #            test/triggers which is executed after dtrace is started.
        #            When this program exits, dtrace will be killed.  If the
        #            timeout expires, both programs are killed.  Arguments may
        #            be provided as normal, and are subjected to shell
        #            expansion.  For the sake of reproducible testcases, only
        #            tests with a trigger, or that explicitly declare that they
        #            can produce reproducible output via /* @@trigger: none */
        #            are ever executed against the kernel.  Triggers should not
        #            emit anything to stdout or stderr, as their output is not
        #            currently trapped and it will spoil the screen display.
        #            See '.trigger'.
        #
        # @@trigger-timing: A single line containing 'before', 'after', or a
        #                   number.  If 'after' (the default) the trigger will
        #                   be exec()ed (from a pre-existing PID) after DTrace
        #                   has started.  If 'before', the trigger will already
        #                   be running when DTrace starts.  If a number,  it is
        #                   a count in seconds to delay trigger execution (to
        #                   wait for DTrace to start).
        #
        # Certain filenames of test .d script are treated specially:
        #
        # tst.*.d: These are assumed to have /* @@trigger: none */ by default.
        #
        # err.*.d: As with tst.*.d, only these are additionally expected to
        #          return 1.  (This is not the same as an XFAIL, which suggests
        #          that this test is known to be not passing: this indicates
        #          that this test is marked PASS if it returns 1, which would
        #          normally indicate failure.)
        #
        # err.D_*.d: As with err.*.d, only run with -xerrtags.
        #
        # drp.*.d: As with tst.*.d, only run -xdroptags.

        # Various other files can exist with the same basename as the test.
        # Many of these serve the same purpose as embedded comments: both exist
        # to permit identical D scripts to be symlinked together while still
        # permitting e.g. different triggering programs to be used.
        #
        # The full list of suffxes is as follows:
        #
        # .d: The D program itself. Either this, or .sh, are mandatory.
        #
        # .sh: If executable, a shell script that is run instead of dtrace with
        #      a single argument, the path to dtrace.  All other features are still
        #      provided, including timeouts.  The script is run as if with
        #      /* @@trigger: none */.  Arguments specified by @@runtest-opts, and
        #      a minimal set without which no tests will work, are passed in
        #      the environment variable dt_flags.
        #
        # .r: Expected results, after postprocessing.  If not present,
        #     no expected-result comparison is done (the results are
        #     merely dumped into the logfile, and the test is deemed to
        #     pass as long as dtrace exits with a zero exitcode or
        #     times out).  If any output is expected on standard error, it
        #     should be appended to this file, after a single line reading
        #     only "-- @@stderr --".  Expected-result comparison can also
        #     be suppressed with the --no-expected command-line parameter.
        #
        #     Files suffixed $(uname -m).r serve to override the results for a
        #     particular architecture.
        #
        # .r.p: Expected-results postprocessor: a filter run before various
        #       hardwired pointer-value-replacement preprocessors which
        #       transform all 0x[a-f]* strings into the string {ptr} and edit
        #       out CPU and probe IDs.
        #       Receives one argument, the name of the testcase.
        #
        # .p: D script preprocessor: a filter run over the D script before
        #     passing to dtrace.  Unlike dtrace's -D options, this can vary
        #     based on the name of the test.
        #
        # .x: If executable, a program which when executed indicates whether
        #     the test is expected to succeed or not in this environment.
        #     0 indicates expected success, 1 expected failure, or 2 that the
        #     test is unreliable and should be skipped; in the latter two cases
        #     it can emit to standard output a one-line message describing the
        #     reason for failure or skipping.  This permits tests to inspect
        #     their environment and determine whether failure is expected, or
        #     whether they can safely run at all.  See '@@xfail' and '@@skip'.
        #     If both .x and @@xfail exist, only the .x is respected.
        #
        #     Files suffixed $(uname -m).x allow programmatic xfails or skips
        #     for a particular architecture.
        #
        # .t: If executable, serves the same purpose as the '@@trigger'
        #     option above.  If both .t and @@trigger exist, only the .t is
        #     respected.

        _exit="${NOEXEC:+-e}"

        # Optionally skip this test.

        if exist_options skip $_test; then
            sum "$_test: SKIP: $(extract_options skip $_test)\n"
            continue
        fi

        # Optionally preprocess the D script.

        if [[ -x $base.p ]]; then
            preprocess $base.p $_test "$tmpdir/$testonly"
            _test="$tmpdir/$testonly"
        fi

        # Per-test timeout.

        if exist_options timeout $_test; then
            timeout="$(extract_options timeout $_test)"
            if [[ -n $VALGRIND ]]; then
                timeout=$((timeout * 10))
            fi
        fi

        # Note if this is expected to fail.
        xfail=
        xfailmsg=
	xfile=$base.$arch.x
	[[ -e $xfile ]] || xfile=$base.x
        if [[ -x $xfile ]]; then
            # xfail program.  Run, and capture its output.
            xfailmsg="$($xfile)"
            case $? in
               0) ;;         # no failure expected
               1) xfail=t;;  # failure expected
               2) sum "$_test: SKIP${xfailmsg:+: $xfailmsg}.\n" # skip
                  continue;;
               *) echo "$xfile: Unexpected return value $?." >&2;;
            esac
        elif exist_options xfail $_test && ! exist_options no-xfail $_test; then
            xfail=t
            xfailmsg="$(extract_options xfail $_test | sed 's, *$,,')"
        fi

        # Check for a trigger.

        trigger=

        # tst.*, err.* and drp.* default to running despite the absence of
        # a trigger.
        if [[ $testonly =~ ^(tst|err|drp)\. ]]; then
            trigger=none
        fi

        # Get a trigger from @@trigger, if possible.
        if exist_options trigger $_test; then
            trigger="$(extract_options trigger $_test t)"
        fi

        # Non-'none' @@triggers must exist in test/triggers.
        if [[ "x$trigger" = "xnone" ]] || [[ -z $trigger ]]; then
            :
        elif [[ ! -x test/triggers/$trigger ]]; then
            trigger=
        else
            trigger=test/triggers/$trigger
        fi

        # Finally, look for a .t file.
        if [[ -x $base.t ]]; then
            trigger=$base.t
        fi

        # Note the expected exitcode of this test.

        expected_exitcode=0
        if [[ $testonly =~ ^err\. ]]; then
            expected_exitcode=1
        fi

        # Default and substitute in flags.  The raw_dt_flags apply even to a
        # sh invocation.

        raw_dt_flags="$test_incflags"

        if [[ $testonly =~ ^err\.D_ ]]; then
            raw_dt_flags="$raw_dt_flags -xerrtags"
        elif [[ $testonly =~ ^drp\. ]]; then
            raw_dt_flags="$raw_dt_flags -xdroptags"
        fi

        dt_flags="$_exit $raw_dt_flags -s $_test"

        if exist_options runtest-opts $_test; then
             opts="$(extract_options runtest-opts $_test t)"

            # The flags go after the dt_flags above, and also land in
            # raw_dt_flags for the sake of shell-script runners.
            if [[ -n $opts ]] && [[ -z $override ]]; then
                dt_flags="$dt_flags $opts"
                raw_dt_flags="$raw_dt_flags $opts"
            fi
        fi

        # Handle an executable .sh.

        shellrun=
        if [[ -x $base.sh ]]; then
            shellrun=$base.sh
            if [[ -z $trigger ]]; then
                trigger=none
            fi
            dt_flags="$raw_dt_flags"
        fi

        # No trigger.  Erase any pre-existing coredump, then un dtrace, with a
        # timeout, without permitting execution, recording the output and
        # exitcode into temporary files.  (We use a different temporary file on
        # every invocation, so that hanging subprocesses emitting output into
        # these files do not mess up subsequent tests.  This won't strictly fix
        # the problem, but will drive its probability of occurrence way down,
        # even in the presence of a hanging test.)

	rm -f core
        failed=
        this_noexec=$NOEXEC
        testmsg=
        testout=$tmpdir/test.out.$RANDOM
        testerr=$tmpdir/test.err.$RANDOM
        out "$_test: "

        if [[ -z $trigger ]]; then
            if [[ -z $shellrun ]]; then
                this_noexec=t
                run_with_timeout $timeout $dt $dt_flags -e > $testout 2> $testerr
            else
                run_with_timeout $timeout $shellrun $dt > $testout 2> $testerr
            fi
            exitcode=$?
        elif [[ "$trigger" = "none" ]]; then
            if [[ -z $shellrun ]]; then
                run_with_timeout $timeout $dt $dt_flags > $testout 2> $testerr
            else
                run_with_timeout $timeout $shellrun $dt > $testout 2> $testerr
            fi
            exitcode=$?
        else
            # A trigger.  Run dtrace with timeout, and permit execution.  First,
            # run the trigger with a 1s delay before invocation, record
            # its PID, and note it in the dt_flags if $_pid is there.
            #
            # We have to run each of these in separate subprocesses to avoid
            # the SIGCHLD from the sleep 1's death leaking into run_with_timeout
            # and confusing it. (This happens even if disowned.)

            trigger_delay=1
            if exist_options trigger-timing $_test; then
                if [[ "x$(extract_options trigger-timing $_test)" = "xbefore" ]]; then
                    trigger_delay=
                elif [[ "x$(extract_options trigger-timing $_test)" = "xafter" ]]; then
                    : # default
                else
		    trigger_delay="$(extract_options trigger-timing $_test)"
                fi
            fi

            log "Running trigger $trigger\n"
            ( [[ -n $trigger_delay ]] && sleep $trigger_delay; exec $trigger; ) &
            _pid=$!
            disown %-
            ZAPTHESE="$_pid"

            if [[ $dt_flags =~ \$_pid ]]; then
                dt_flags="$(echo ''"$dt_flags" | sed 's,\$_pid[^a-zA-Z],'$_pid',g; s,\$_pid$,'$_pid',g')"
            fi

            if [[ -z $shellrun ]]; then
                ( run_with_timeout $timeout $dt $dt_flags > $testout 2> $testerr
                  echo $? > $tmpdir/dtrace.exit; )
            else
                ( run_with_timeout $timeout $shellrun $dt > $testout 2> $testerr
                  echo $? > $tmpdir/dtrace.exit; )
            fi
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

        # Note if dtrace mentions running out of memory at any point.
        # If it does, this test quietly becomes an expected failure
        # (without transforming an err.* test into an XPASS).
        if grep -q "Cannot allocate memory" $testout $testerr; then
            xfailmsg="out of memory"

            if [[ $expected_exitcode -eq 0 ]]; then
                xfail=t
            fi
        fi

        if [[ -n $this_noexec ]]; then
            testmsg="no execution"
        fi

        if [[ -s $testerr ]]; then
            echo "-- @@stderr --" >> $testout
            cat $testerr >> $testout
        fi

        if ! postprocess $base.r.p $testout $tmpdir/test.out; then
            testmsg="results postprocessor failed with exitcode $?"
        fi
        rm -f $testout $testerr

        # Note if we will certainly capture results.

        capturing=
        if [[ -n $CAPTURE_EXPECTED ]] && [[ -n $OVERWRITE_RESULTS ]] &&
           [[ -n $COMPARISON ]]; then
            capturing="results captured"
        fi

        # Results processing is complicated by the existence of a
        # class-of-failure hierarchy (e.g. coredumps should be reported in
        # preference to comparison failures, but we still want comparison
        # failures to be logged) and by the need to print a message about each
        # failure at most once, and by our desire to print any expected-results
        # diff after the logged output (so it stands out).
        want_expected_diff=
        want_all_output=
        fail=
        failmsg=

        # Compare results, if available, and log the diff.
	rfile=$base.$arch.r
	[[ -e $rfile ]] || rfile=$base.r

        if [[ -e $rfile ]] && [[ -n $COMPARISON ]] &&
           ! diff -u <(sort $rfile) <(sort $tmpdir/test.out) >/dev/null; then

            fail=t
            failmsg="expected results differ"
            want_expected_diff=t
        elif [[ ! -e $base.r ]]; then
            # No expected results?  Write all the output to the sumfile.
            # (It is also always written to the logfile: see below.)
            want_all_output=t
        fi

        if [[ -f core ]]; then
            # A coredump. Preserve it in the logdir.

            mv core $logdir/$(echo $base | tr '/' '-').core
            fail=t
            failmsg="core dumped"

        # Exitcodes are not useful if there's been a coredump, but otherwise...
        elif [[ $exitcode != $expected_exitcode ]] && [[ $exitcode != 126 ]]; then

            # Some sort of exitcode error.  Assume that errors in the
            # range 129 -- 193 (a common value of SIGRTMAX) are signal
            # exits.

            fail=t
            if [[ $exitcode -lt 129 ]] || [[ $exitcode -gt 193 ]]; then
                failmsg="erroneous exitcode ($exitcode)"
            else
                failmsg="hit by signal $((exitcode - 128))"
            fi
        fi

        if [[ -z $fail ]]; then

            # Success!
            # If results don't already exist and we are in capture mode, then
            # capture them even if forcible capturing is off.

            if [[ -n $CAPTURE_EXPECTED ]] && [[ -n $COMPARISON ]] &&
               [[ ! -e $base.r ]]; then
                capturing="results captured"
            fi

            pass "$xfail" "$xfailmsg" "$capturing"

        else
            fail "$xfail" "$xfailmsg" "$failmsg${capturing:+, $capturing}"
        fi

        # Always log the test output.
        cat $tmpdir/test.out >> $LOGFILE

        if [[ -n $want_all_output ]]; then
            cat $tmpdir/test.out >> $SUMFILE

        elif [[ -n $want_expected_diff ]]; then
            log "Diff against expected:\n"

            diff -u $rfile $tmpdir/test.out | tee -a $LOGFILE >> $SUMFILE
        fi

        # If capturing results is requested, capture them now.
        if [[ -n $capturing ]]; then
            cp -f $tmpdir/test.out $base.r
        fi

        log "\n"

        if [[ -n $regression ]]; then
            # If regtesting, we run a second time, with intermediate results
            # displayed, and output redirected to a per-test, per-dtrace
            # temporary file. We always ban execution during regtesting:
            # our real interest is whether intermediate results have changed.

            dtest_dir="$tmpdir/regtest/$(basename $(dirname $dt))"
            reglog="$dtest_dir/$(echo $_test | sed 's,/,-,g').log"
            mkdir -p $dtest_dir
            echo "$dt_flags -e" > $reglog
            echo >> $reglog
            $dt -S $dt_flags -e > $tmpdir/regtest.out 2>&1 
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

    force_out "\n\nRegression test comparison.\n"

    for name in $tmpdir/regtest/*; do
        [[ $name = $first_dtest_dir ]] && continue

        if ! diff -urN $first_dtest_dir $name | tee -a $LOGFILE $SUMFILE; then
            force_out "Regression test comparison failed."
        fi
    done
else
    # Test summary.

    awk -f - $SUMFILE <<'EOF' | tee -a $LOGFILE $SUMFILE
/: X?(FAIL|PASS|SKIP)/ {
	match($0, /: X?(FAIL|PASS|SKIP)/);
	rc = substr($0, RSTART + 2, RLENGTH - 2);
	count[rc]++;
	total++;
 }
 END {
	printf("%d cases (%d PASS, %d FAIL, %d XPASS, %d XFAIL, %d SKIP)\n",
		total, count["PASS"], count["FAIL"], count["XPASS"],
		       count["XFAIL"], count["SKIP"]);
}
EOF

fi

# Now unload modules before acquiring coverage info.
unload_modules

# Test coverage.
for name in build*; do
    if [[ -n "$(echo $name/*.gcda)" ]]; then
        force_out "Coverage info for $(echo $name | sed 's,build,userspace,'):\n"
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
    force_out "Coverage info for kernel:\n"

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
