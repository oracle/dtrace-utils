#!/bin/bash
#
# runtest.sh -- A dtrace tester and regression tester.
#
#               Runs a bunch of .d scripts and tests their results.
#               If more than one dtrace build directory is available,
#               runs them each in turn, comparing their output
#               and generated intermediate representation.
#
# Oracle Linux DTrace.
# Copyright (c) 2011, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# Sanitize the shell and get configuration info.

shopt -s nullglob extglob
unset CDPATH
unset POSIXLY_CORRECT  # Interferes with 'wait'
export LC_COLLATE="C"
export dtrace="/usr/sbin/dtrace"

arch="$(uname -m)"

load_modules()
{
    # If running as root, pull in appropriate modules
    if [[ "x$(id -u)" = "x0" ]]; then
	cat test/modules | xargs -rn 1 modprobe 2>/dev/null
    fi
}

#
# Utility function to ensure a given provider is present.
# Uses the providers list written out at start time.
#
check_provider()
{
    grep -q "^$1\$" $tmpdir/providers
}

export -f check_provider

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
            chattr +D $1/$i 2>/dev/null
            rm -f $1/current
            ln -s $i $1/current
            echo $1/$i
            return
        fi
    done
}

declare -a ZAPTHESE=

# run_with_timeout TIMEOUT CMD [ARGS ...]
#
# Run command CMD with argument ARGS, with the specified timeout.
# Return the exitcode of the command, or $TIMEOUTRET if a timeout occurred.
# Also send a KILL signal if the command is still running TIMEOUTKIL seconds
# after the initial signal is sent, but start with TIMEOUTSIG so that DTrace
# can perform an orderly shutdown, if possible.

TIMEOUTSIG=TERM
TIMEOUTRET=124
TIMEOUTKIL=5

run_with_timeout()
{
    log "Running timeout --signal=$TIMEOUTSIG $@ ${explicit_arg:+"$explicit_arg"}\n"
    timeout --signal=$TIMEOUTSIG --kill-after=$TIMEOUTKIL $@ ${explicit_arg:+"$explicit_arg"}
    local status=$?

    # short wait to allow any coredump to trickle out to disk.
    [[ $status -ne 0 ]] && sleep 1

    # status 128+9 indicates timeout via KILL(=9)
    if [[ $status -eq 137 ]]; then
        status=$TIMEOUTRET
    fi
    return $status
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

# extract_options NAME FILE EXPAND ACCUMULATE
#
# Extract the options named NAME from FILE, or in the test.options file in its
# directory or any parent up to test/, and return them on stdout.
#
# If EXPAND is set, do a round of shell evaluation on them first.
#
# If ACCUMULATE is set, return the concatenated values of all options named
# NAME up to test/.

extract_options()
{
    local retval=1
    local path=$2
    local file=$2
    local expand=$3
    local accumulate=$4
    local val=

    while :; do
        # This horrible sequence of patterns catches @@foo, @@foo: bar, and @@foo */,
        # while not catching @@foo-bar or @@foo-bar: baz. */
        if [[ -f $file ]] && grep -Eq -e "@@"$1' *:' -e "@@"$1" " -e "@@"$1'$' $file; then
            local val="$val$(grep -E -e "@@"$1' *:' -e "@@"$1" " -e "@@"$1'$' $file | sed 's,.*@@'$1' *: ,,; s,\*/$,,; s,  *$,,')"
            if [[ -n $accumulate ]]; then
                val="$val "
            fi
        fi
        path="$(dirname $path)"
        file="$path/test.options"
        if [[ -e "$path/test.$arch.options" ]]; then
            file="$path/test.$arch.options"
        fi
        # Halt when we have any tags if not accumulating, or at top level anyway.
        if { [[ -z $accumulate ]] && [[ -n $val ]]; } || [[ -e $path/Makecheck ]]; then
            # Force the $_pid to expand to itself.

            _pid='$_pid'

            # This trick is because printf with a straight eval ends up called
            # with more args than format string items and squashes out spaces,
            # echo -e -e foo prints 'foo', and bash's builtin echo -e -- "foo"
            # prints "-- foo".  coreutils echo does the right thing, but is more
            # expensive than the builtin printf, so use it only when needed.

            if [[ -n $val ]]; then
                if [[ -n $expand ]]; then
                    eval printf "%s" \""$val"\"
                else
                    printf "%s" ''"$val"
                fi
            fi
            return
        fi
    done
}

# is_interpreter_file FILE

is_interpreter_file()
{
    if [[ ! -e $1 ]]; then
        return 1
    fi
    if [[ $(gawk '{print $1; exit}' $1) != "#!dtrace" ]]; then
        return 1
    fi

    # At this point, perhaps we can also check that the file is executable.
    # But we will copy it anyhow.  So do not bother.

    return 0
}

DEFAULT_TIMEOUT=41

usage()
{
    cat <<EOF
$0 -- testsuite engine for DTrace

Usage: runtest options [TEST ...]

Options:
 --timeout=TIME: Time out test runs after TIME seconds (default
                 $DEFAULT_TIMEOUT). (May cause many test failures if lowered.)
 --skip-longer[=TIME]: Skip all tests with expected timeouts longer than
                       TIME (if not specified, $DEFAULT_TIMEOUT or whatever is
                       specified by --timeout, whichever is longer).
 --ignore-timeouts: Treat all timeouts as not being failures.  (For use when
                    testing under load.)
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
 --[no-]use-installed: Use an installed dtrace rather than a copy in the
                       source tree.
 --quiet: Only show unexpected output (FAILs and XPASSes).
 --verbose: The opposite of --quiet (and the default).
 --[no-]tag=TAG: Run only tests with[out] TAG.
                 Multiple such options are ANDed together.
 --help: This message.

If one or more TESTs is provided, they must be the name of .d files existing
under the test/ directory.  If none is provided, all available tests are run.
EOF
exit 0
}

# Parameters.

CAPTURE_EXPECTED=${DTRACE_TEST_CAPTURE_EXPECTED:+t}
OVERWRITE_RESULTS=
NOEXEC=${DTRACE_TEST_NO_EXECUTE:+t}
USE_INSTALLED=${DTRACE_TEST_USE_INSTALLED:+t}
VALGRIND=${DTRACE_TEST_VALGRIND:+t}
COMPARISON=t
SKIP_LONGER=

if [[ -n $DTRACE_TEST_TESTSUITES ]]; then
    TESTSUITES="${DTRACE_TEST_TESTSUITES}"
else
    TESTSUITES="unittest internals stress demo"
fi

ONLY_TESTS=
TESTS=
QUIET=
USER_TAGS=

if [[ -n $DTRACE_TEST_TIMEOUT ]]; then
    TIMEOUT="$DTRACE_TEST_TIMEOUT"
else
    TIMEOUT=$DEFAULT_TIMEOUT
fi
IGNORE_TIMEOUTS=${DTRACE_TEST_IGNORE_TIMEOUTS:+t}

ERRORS=

while [[ $# -gt 0 ]]; do
    case $1 in
        --capture-expected) CAPTURE_EXPECTED=t;;
        --no-capture-expected) CAPTURE_EXPECTED=;;
        --execute) NOEXEC=;;
        --no-execute) NOEXEC=t;;
        --comparison) COMPARISON=t;;
        --no-comparison) COMPARISON=;;
        --valgrind) VALGRIND=t;;
        --no-valgrind) VALGRIND=;;
        --use-installed) USE_INSTALLED=t;;
        --no-use-installed) USE_INSTALLED=;;
        --timeout=*) TIMEOUT="$(printf -- $1 | cut -d= -f2-)";;
        --ignore-timeouts) IGNORE_TIMEOUTS=t;;
	--skip-longer) SKIP_LONGER=$TIMEOUT;;
	--skip-longer=*) SKIP_LONGER="$(printf -- $1 | cut -d= -f2-)";;
        --testsuites=*) TESTSUITES="$(printf -- $1 | cut -d= -f2- | tr "," " ")";;
        --quiet) QUIET=t;;
        --verbose) QUIET=;;
        --tag=*) USER_TAGS="$USER_TAGS $(printf -- $1 | cut -d= -f2-)";;
        --no-tag=*) USER_TAGS="$USER_TAGS !$(printf -- $1 | cut -d= -f2-)";;
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

# Set a locked-memory limit that should be big enough for the test suite.

ulimit -l $((256 * 1024 * 1024))

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

# At shutdown, delete this directory, kill requested processes, and restore
# core_pattern.  When this is specifically an interruption, report as much in
# the log.
closedown()
{
    for pid in ${ZAPTHESE[@]}; do
        kill -9 -- $pid
    done
    if [[ -z $orig_core_pattern ]]; then
        echo $orig_core_pattern > /proc/sys/kernel/core_pattern
        echo $orig_core_uses_pid > /proc/sys/kernel/core_uses_pid
    fi
    declare -ga ZAPTHESE=
}

trap 'rm -rf ${tmpdir}; closedown; exit' EXIT
trap 'force_out "Test interrupted.\n"; closedown' INT HUP QUIT TERM

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

    sed -e '/^==[0-9][0-9]*== /!s,0x[0-9a-f][0-9a-f]*,{ptr},g' \
	-e 's,at BPF pc [1-9][0-9]*,at BPF pc NNN,' < $tmpdir/pp.out > $final

    return $retval
}

# Log results. (Purely to reduce duplication further down.)
# The single argument indicates whether to write to the sumfile:
# writes to the logfile due to reinvocations should not go to the sumfile,
# since that is just noise and throws off results counting.
resultslog()
{
    want_sum=$1
    # Always log the unpostprocessed test output.
    cat $testout >> $LOGFILE

    rm -f $testout

    if [[ -n $want_sum ]]; then
        if [[ -n $want_all_output ]]; then
            cat $tmpdir/test.out >> $SUMFILE

        elif [[ -n $want_expected_diff ]]; then
            log "Diff against expected:\n"

            diff -au $rfile $tmpdir/test.out | tee -a $LOGFILE >> $SUMFILE
        fi
    fi

    # Finally, write the debugging output, if any, to the logfile.
    if [[ -s $testdebug ]]; then
        echo "-- @@debug --" >> $LOGFILE
        cat $testdebug >> $LOGFILE
    fi
    rm -f $testdebug
}

# Flip a few env vars to tell dtrace to produce reproducible output.
export _DTRACE_TESTING=t
export LANGUAGE=C

if [[ -z $USE_INSTALLED ]]; then
    dtrace="$(pwd)/build*/dtrace"
    test_libdir="$(pwd)/build/dlibs"
    test_ldflags="-L$(pwd)/build"
    test_cppflags="-I$(pwd)/include -I$(pwd)/uts/common -I$(pwd)/build -I$(pwd)/libdtrace -DARCH_$arch"
    helper_device="dtrace/test-$$"
    # Pre-existing directories from earlier tests are just fine!
    # dtprobed will clean things up.
    mkdir -p $tmpdir/run/dtrace
    dtprobed_flags="-n $helper_device -s${tmpdir}/run/dtrace -F"
    export DTRACE_DOF_INIT_DEVNAME="/dev/$helper_device"
    export DTRACE_OPT_DOFSTASHPATH="${tmpdir}/run/dtrace"

    if [[ -z $(eval echo $dtrace) ]]; then
    	echo "No dtraces available." >&2
    	exit 1
    fi
    build/dtprobed $dtprobed_flags &
    export dtprobed_pid=$!
    ZAPTHESE+=($dtprobed_pid)
else
    # If we don't have a pkg-config path, try to point at a plausible
    # one given DTrace's default install paths.  This will fail if
    # the pkg-config path is changed as well: in that case, presumably
    # the person changing that path has pointed pkg-config at it anyway.
    if [[ -z $PKG_CONFIG_PATH ]]; then
        export PKG_CONFIG_PATH="$(pwd)/../../../share/pkgconfig"
    fi

    dtrace="$(pkg-config --variable=dtrace dtrace)"
    test_libdir="installed"
    test_ldflags=""
    test_cppflags="-DARCH_$arch $(pkg-config --cflags dtrace_sdt) $(pkg-config --cflags dtrace)"

    if [[ ! -x $dtrace ]]; then
        echo "$dtrace not available." >&2
        exit 1
    fi
fi
export test_cppflags
export test_ldflags
export test_libdir

# Figure out if the preprocessor supports -fno-diagnostics-show-option: if it
# does, add a bunch of options designed to make GCC output look like it used
# to.
# Regardless, turn off display of column numbers, if possible: they vary
# between GCC releases.

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
# in the right place.
if [[ "x$test_libdir" != "xinstalled" ]]; then
    export DTRACE_OPT_SYSLIBDIR="$test_libdir"
fi

# Determine the list of available providers
for dt in $dtrace; do
    # Look for our shared libraries in the right place.
    if [[ "x$test_libdir" != "xinstalled" ]]; then
        export LD_LIBRARY_PATH="$(dirname $dt)"
    fi

    # Write out a list of loaded providers.
    DTRACE_DEBUG= $dt -l | tail -n +2 | gawk '{print $2;}' | sort -u > $tmpdir/providers

    unset LD_LIBRARY_PATH
    break
done

# Initialize test coverage.

for name in build*; do
    if [[ -n "$(echo $name/*.gcno)" ]]; then
        rm -rf $logdir/coverage
        mkdir -p $logdir/coverage
        lcov --zerocounters --directory $name --quiet
        lcov --capture --base-directory . --directory $name --initial \
             --quiet -o $logdir/coverage/initial.lcov 2>/dev/null
    fi
done

load_modules

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

# Close unexpectedly open FDs.
for fd in /proc/$$/fd/*; do
    fd="${fd##*/}"
    case $fd in
        0|1|2|255) continue;;
        *) exec {fd}>&-;;
    esac
done

# Mark the log and sumfiles as fsynced always.

touch $LOGFILE
touch $SUMFILE
chattr +S $LOGFILE $SUMFILE 2>/dev/null

# Form run and skip files from multiple sources of user-specified tags:
#   - default-tag file (test/tags.default)
#   - per-arch default-tag file (test/tags.$arch.default
#   - environment variable (TEST_TAGS)
#   - command-line options (--[no-]tag=)

USER_TAGS="$(cat test/tags.default test/tags.$arch.default 2>/dev/null) $TEST_TAGS $USER_TAGS"
rm -f $tmpdir/run.tags > /dev/null 2>&1
rm -f $tmpdir/skip.tags > /dev/null 2>&1
printf "%s\n" $USER_TAGS | grep -v '^!' | sort -u > $tmpdir/run.tags
printf "%s\n" $USER_TAGS | grep '^!' | sed 's/^!//' | sort -u > $tmpdir/skip.tags
# Free ourselves of the hassles of empty (but existing) files.
for name in $tmpdir/run.tags $tmpdir/skip.tags; do
    if [[ `cat $name | wc -w` -eq 0 ]]; then
        rm $name
    fi
done

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
    DTRACE_DEBUG= $dt -vV | tee -a $LOGFILE >> $SUMFILE
    uname -a | tee -a $LOGFILE >> $SUMFILE
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
        vg="valgrind -q "
        chmod a+x $tmpdir/bin/$(basename $dt)
    else
        vg=""
    fi

    for _test in $(if [[ $ONLY_TESTS ]]; then
                      echo $TESTS | sed 's,\.r$,\.d,g; s,\.r ,.d ,g'
                   else
                      for name in $TESTSUITES; do
                          find test/$name \( -name "*.d" -o -name "*.sh" -o -name "*.c" \) | sort -u
                      done
                   fi); do

        base=${_test%.d}
        base=${base%.sh}
        base=${base%.c}
        testonly="$(basename $_test)"
        export timeout="$TIMEOUT"

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
        # value for that option.  If a file named test.$(uname -m).options
        # exists at a given level, it is consulted instead of test.options at
        # that level.
        #
        # @@runtest-opts: A set of options to be added to the default set
        #                 (normally -S -e -s, where the -S and -e may not
        #                 necessarily be provided.) Subjected to shell
        #                 expansion.
        #
        # @@timeout: The timeout to use for this test.  Overrides the --timeout
        #            parameter, iff the @@timeout is greater.
        #
        # @@skip: If true, the test is skipped.
        #
        # @@tags: Tag test case with specified tags, separated by spaces.
        #         Tags named after all subdirectories below 'test' are
        #         automatically added. (e.g. all tests in test/unittest/proc
        #         will gain the tags test/unittest/proc and test/unittest.)
        #
        # @@xfail: A single line containing a reason for this test's expected
        #          failure.  (If the test passes unexpectedly, this message is
        #          not printed.) See '.x'.
        #
        # @@no-xfail: If present, this means that a test is *not* expected to
        #             fail, even though a test.options in a directory above
        #             this test says otherwise.
        #
        # @@timeout-success: If present, this means that a timeout is
        #                    considered a test pass, not a test failure.
        #
        # @@deadman-success: If present, this means that a deadman timer
        #                    firing is considered a test pass, not a test
        #                    failure.
        #
        # @@reinvoke-failure: If present, this is an integer indicating the
        #                     number of times to reinvoke failed tests: only
        #                     if they fail every time will they be considered
        #                     a FAIL rather than a PASS.
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
        # @@trigger-timing: A single line containing 'before', 'after',
        #                   'synchro', or a number.  If 'synchro' (the
        #                   default), the trigger will be passed to dtrace via
        #                   -c and left for dtrace to invoke.  If 'after', the
        #                   trigger will be exec()ed (from a pre-existing PID)
        #                   after DTrace has started.  If 'before', the trigger
        #                   will already be running when DTrace starts.  If a
        #                   number, it is a count in seconds to delay trigger
        #                   execution (to wait for DTrace to start).
        #
        # @@link: A library to link .c programs against (see below).  -ldtrace
        #         by default.
        #
        # @@nosort: If present, this means do not sort before comparing results
        #           with a .r file.  Results are sorted by default, since many
        #           test results do not have a well-defined order -- notably,
        #           when generated on different CPUs.  Some tests, however,
        #           care about strict ordering.
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
        # .d: The D program itself.
        #
        # .sh: If executable, a shell script that is run instead of dtrace with
        #      a single argument, the path to dtrace.  All other features are still
        #      provided, including timeouts.  The script is run as if with
        #      /* @@trigger: none */.  Arguments specified by @@runtest-opts, and
        #      a minimal set without which no tests will work, are passed in
        #      the environment variable dt_flags.  An exit code of 67 will cause
        #      the test to be considered an expected failure.
        #
        # .c: A C program that is linked against the libraries listed in @@link,
        #     or -ldtrace by default, then run.
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
        #     Files suffixed $(uname -m).x allow xfails or skips for a
        #     particular architecture.
        #
        #     As with test.options, executable files named test.x and
        #     test.$(uname -m).x can exist at any level in the hierarchy:
        #     the one that is 'most faily' wins (so any SKIP overrides any
        #     XFAIL, which overrides any PASS, no matter what level of the
        #     hierarchy it is found at).
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
        
        # Check if the user specified any tags.

        if [[ -n "$USER_TAGS" ]]; then

            # Extract tags from test, and add tags derived from the test's containing
            # directory.
            rm -f $tmpdir/test.tags >/dev/null 2>&1
            test_tags="$(extract_options tags $_test "" t)"
            test_tags="$test_tags $(shrinktest=$_test
                                    while :; do
                                        shrinktest="${shrinktest%/*}"
                                        [[ "$shrinktest" =~ / ]] || break
                                        printf "%s " "$shrinktest"
                                    done)"
            printf "%s\n" $test_tags | sort -u > $tmpdir/test.tags

            # If user specified run tags, check for them.
            if [[ -e $tmpdir/run.tags ]]; then
                if [[ -n "$(comm -23 $tmpdir/run.tags $tmpdir/test.tags)" ]]; then
                    # Some user-specified run tags are not in test.  Skip test.
                    tagdiff=$(comm -23 $tmpdir/run.tags $tmpdir/test.tags | xargs)
                    sum "$_test: SKIP: test lacks requested tags: $tagdiff\n"
                    continue
                fi
            fi

            # If user specified skip tags, check for them.
            if [[ -e $tmpdir/skip.tags && -n $test_tags ]]; then
                if [[ -n "$(comm -12 $tmpdir/skip.tags $tmpdir/test.tags)" ]]; then
                    # Some user-specified skip tags are in test.  Skip test.
                    tagdiff=$(comm -12 $tmpdir/skip.tags $tmpdir/test.tags | xargs)
                    sum "$_test: SKIP: test has excluded tags: $tagdiff\n"
                    continue
                fi
            fi
        fi

        # Per-test timeout.

        if exist_options timeout $_test; then
            if [[ "$(extract_options timeout $_test)" -gt $timeout ]]; then
                timeout="$(extract_options timeout $_test)"

                if [[ -n $VALGRIND ]]; then
                    timeout=$((timeout * 10))
                fi
            fi
        fi

        # Timeout-based skipping.

        if [[ -n $SKIP_LONGER ]] && [[ $timeout -gt $SKIP_LONGER ]]; then
            sum "$_test: SKIP: would take too long\n"
            continue
        fi

        # Note if this is expected to fail.
        xfail=0
        xfailpath="$_test"
        xbase=$base
        xfailmsg=

        if exist_options xfail $_test && ! exist_options no-xfail $_test; then
            xfail=1
            xfailmsg="$(extract_options xfail $_test | sed 's, *$,,')"
        fi

        # Scan for $test.x, $test.$arch.x, then test.x and test.arch.x
        # all the way up to the top level, much as for test.options.
        # Whichever .x file returns the highest value is used (SKIP >
        # XFAIL > PASS).
        while [[ $xfail -lt 2 ]]; do
            xfile=$xbase.$arch.x
            [[ -e $xfile ]] || xfile=$xbase.x
            if [[ -x $xfile ]]; then
                # xfail program.  Run, and capture its output, clamping the
                # exitcode at 2.  (The output of whichever program has the
                # highest exitcode is used.)
                xfailmsgthisfile="$($xfile)"
                xfailthisfile=$?
                if [[ $xfailthisfile -gt 2 ]]; then
                   echo "$xfile: Unexpected return value $xfailthisfile." >&2
                   xfailthisfile=2
                fi
                if [[ $xfailthisfile -gt $xfail ]]; then
                    xfail=$xfailthisfile
                    xfailmsg="$xfailmsgthisfile"
                fi
            elif [[ -e $xfile ]] && [[ $xfail -eq 0 ]]; then
                xfail=1
            fi
            xfailpath="$(dirname $xfailpath)"
            xbase="$xfailpath/test"
            if [[ -e "$xfailpath/test.$arch.x" ]]; then
                xbase="$xfailpath/test.$arch"
            fi
            # Halt at top level.
            if [[ -e $xfailpath/Makecheck ]]; then
                break
            fi
        done

        case $xfail in
           0) xfail="";; # no failure expected
           1) xfail=t;;  # failure expected
           2) sum "$_test: SKIP${xfailmsg:+: $xfailmsg}.\n" # skip
              continue;;
           *) xfail="";
              echo "$xfile: Unexpected return value $?." >&2;;
        esac

        # Failure reinvocation.
        reinvoke_failure=0
        reinvoked=0
        if exist_options reinvoke-failure $_test; then
            reinvoke_failure="$(extract_options reinvoke-failure $_test)"
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
        elif [[ ! -x test/triggers/$(printf '%s' "$trigger" | cut -d\  -f1) ]]; then
            out "$_test: "
            fail=t
            trigger=$(printf '%s' "$trigger" | cut -d\  -f1)
            fail "$xfail" "$xfailmsg" "trigger $trigger not found in test/triggers"
            continue
        else
            trigger="test/triggers/$trigger"
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

        raw_dt_flags="$test_cppflags"

        expected_tag=
        if [[ $testonly =~ ^err\.D_ ]]; then
            raw_dt_flags="$raw_dt_flags -xerrtags"
            expected_tag="$(echo ''"$testonly" | sed 's,^err\.,,; s,\..*$,,')"
        elif [[ $testonly =~ ^drp\. ]]; then
            raw_dt_flags="$raw_dt_flags -xdroptags"
            expected_tag="$(echo ''"$testonly" | sed 's,^drp\.,,; s,\..*$,,')"
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

        progtype=d
        run=$dt
        interpreter_file=""
        if is_interpreter_file $base.d; then
            interpreter_file=$tmpdir/test.interpreter.$RANDOM.d
            rm -f $interpreter_file
            sed 's:^#!dtrace :#!'$dt' :' $base.d > $interpreter_file
            chmod +x $interpreter_file
            progtype=shell
            run="$interpreter_file $raw_dt_flags"
        elif [[ -x $base.sh ]]; then
            progtype=shell
            run="$base.sh $dt"
            if [[ -z $trigger ]]; then
                trigger=none
            fi
            dt_flags="$raw_dt_flags"
        elif [[ -e $base.c ]]; then
            progtype=c
            run="$vg $tmpdir/$(basename $base)"
        fi

        # Erase any pre-existing coredump, then run dtrace, with a
        # timeout, without permitting execution, recording the output and
        # exitcode into temporary files.  (We use a different temporary file on
        # every invocation, so that hanging subprocesses emitting output into
        # these files do not mess up subsequent tests.  This won't strictly fix
        # the problem, but will drive its probability of occurrence way down,
        # even in the presence of a hanging test. stdout and stderr go into
        # different files, and any debugging output is split into a third.)

        rm -f core
        fail=
        this_noexec=$NOEXEC
        testmsg=
        testout=$tmpdir/test.out.$RANDOM
        testerr=$tmpdir/test.err.$RANDOM
        testdebug=$tmpdir/test.debug.$RANDOM
        out "$_test: "

        if [[ "$progtype" = "c" ]]; then
            if [[ -z "$(extract_options link $_test)" ]]; then
                link="-ldtrace"
            else
                link="$(extract_options link $_test)"
            fi

            CC="${CC:-gcc}"
            CCline="$CC -std=gnu99 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64"
            CCline="$CCline $test_cppflags $CFLAGS $test_ldflags $LDFLAGS"
            CCline="$CCline -o $tmpdir/$(basename $base) $_test $link"
            log "Compiling $CCline\n"
            if ! $CCline >/dev/null 2>$tmpdir/cc.err; then
                fail=t
                fail "$xfail" "$xfailmsg" "compilation failure"
                cat $tmpdir/cc.err >> $LOGFILE
                continue
            fi
        fi

        orig_dt_flags="$dt_flags"
        while [[ $reinvoke_failure -ge 0 ]]; do
            fail=
            tst=$base
            export tst
            if [[ -z $trigger ]] || [[ "$trigger" = "none" ]]; then
                # No trigger.
                case $progtype in
                    d) eflag=
                       if [[ -z $trigger ]]; then
                            eflag=-e
                            this_noexec=t
                       fi
                       run_with_timeout $timeout $run $dt_flags $eflag > $testout 2> $testerr;;
                    shell) run_with_timeout $timeout $run > $testout 2> $testerr;;
                    c) run_with_timeout $timeout $run > $testout 2> $testerr;;
                    *) out "$_test: Internal error: unknown program type $progtype";;
                esac
                exitcode=$?
            else
                # A trigger.  Run dtrace with timeout, and permit execution.  First,
                # run the trigger with a 1s delay before invocation, record
                # its PID, and note it in the dt_flags if $_pid is there.
                #
                # We have to run each of these in separate subprocesses to avoid
                # the SIGCHLD from the sleep 1's death leaking into run_with_timeout
                # and confusing it. (This happens even if disowned.)
                #
                # For .c tests, none of this is supported: we just run the trigger
                # first and kill it afterwards, as if @@trigger-timing: before,
                # passing the PID to the C program as its lone parameter.

                trigger_timing=synchro
                trigger_delay=
                needszap=
                if exist_options trigger-timing $_test &&
                   [[ "x$(extract_options trigger-timing $_test)" != "xsynchro" ]]; then
                    trigger_timing="$(extract_options trigger-timing $_test)"
                    case $trigger_timing in
                        after) trigger_delay=1;;
                        before|synchro) ;;
                        *) trigger_delay=$trigger_timing;;
                    esac
                fi

                explicit_arg=
                if [[ "$trigger_timing" == "synchro" ]] && [[ $progtype != "c" ]]; then
                    # This is an unbearably ugly hack to try to kludge around
                    # shell quoting difficulties.
                    dt_flags="$dt_flags -c "
                    explicit_arg="$trigger"
                    _pid=
                else
                    log "Running trigger $trigger${trigger_delay:+ with delay $trigger_delay}\n"
                    ( [[ -n $trigger_delay ]] && sleep $trigger_delay; exec $trigger; ) &
                    _pid=$!
                    disown %-
                    needszap=t
                    ZAPTHESE+=($_pid)

                    if [[ $dt_flags =~ \$_pid ]]; then
                        dt_flags="$(echo ''"$dt_flags" | sed 's,\$_pid[^a-zA-Z],'$_pid',g; s,\$_pid$,'$_pid',g')"
                    fi
                fi

                case $progtype in
                    d) ( run_with_timeout $timeout $run $dt_flags > $testout 2> $testerr
                         echo $? > $tmpdir/dtrace.exit; );;
                    shell) ( run_with_timeout $timeout $run > $testout 2> $testerr
                             echo $? > $tmpdir/dtrace.exit; );;
                    c) ( run_with_timeout $timeout $run $_pid > $testout 2> $testerr
                         echo $? > $tmpdir/dtrace.exit; );;
                    *) out "$_test: Internal error: unknown program type $progtype";;
                esac
                exitcode="$(cat $tmpdir/dtrace.exit)"
                explicit_arg=

                # If the trigger is still running, kill it, and wait for it, to
                # quiesce the background-process-kill noise the shell would
                # otherwise emit.
                if [[ -n $_pid ]] && [[ "$(ps -p $_pid -o ppid=)" -eq $BASHPID ]]; then
                    kill $_pid >/dev/null 2>&1
                fi
                if [[ -n $needszap ]]; then
                    unset "ZAPTHESE[$((${#ZAPTHESE[@]}-1))]"
                fi
                unset _pid
            fi

            # Translate $interpreter_file back into name of test file.
            if [[ -n $interpreter_file ]]; then
                sed -i 's:'$interpreter_file':'$base.d':' $testout
                sed -i 's:'$interpreter_file':'$base.d':' $testerr
            fi

            # Split debugging info out of the test output.
            grep -E '^[a-z_]+ DEBUG [0-9]+: ' $testerr > $testdebug
            grep -vE '^[a-z_]+ DEBUG [0-9]+: ' $testerr > $testerr.tmp

            # Account for an error message change in CTF
            sed -e 's/Invalid member name/Member name not found/' $testerr.tmp > $testerr
            rm -f $testerr.tmp

            # Note if dtrace mentions running out of memory at any point.
            # If it does, this test quietly becomes an expected failure
            # (without transforming an err.* test into an XPASS).
            if grep -q "Cannot allocate memory" $testout $testerr; then
                xfailmsg="out of memory"

                if [[ $expected_exitcode -eq 0 ]]; then
                    xfail=t
                fi
            fi

            # If deadman-success is on, then a deadman timer firing turns
            # a test into an XFAIL.
            if grep -q "Abort due to systemic unresponsiveness" $testerr &&
                [[ "x$(extract_options deadman-success $_test)" != "x" ]]; then
                testmsg="(expected) systemic unresponsiveness"
                if [[ $expected_exitcode -eq 0 ]]; then
                    xfail=t
                fi
            fi

            # If we are expecting a specific tag in the error output, make sure
            # it is present.  If found, we clear expected_tag to signal that
            # all is fine.
            if [[ -n $expected_tag ]] &&
                grep -q "\[$expected_tag\]" $testerr; then
                expected_tag=
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
                fail=t
            fi

            rm -f $testerr

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
            failmsg=

            # By default, sort before comparing since test results often
            # have indeterminate order (generated on different CPUs, etc.).
            if exist_options nosort $_test; then
                sortcmd=cat
            else
                sortcmd=sort
            fi

            # Compare results, if available, and log the diff.
            rfile=$base.$arch.r
            [[ -e $rfile ]] || rfile=$base.r

            if [[ -e $rfile ]] && [[ -n $COMPARISON ]] &&
               ! diff -u <($sortcmd $rfile) <($sortcmd $tmpdir/test.out) >/dev/null; then

                fail=t
                failmsg="expected results differ"
                want_expected_diff=t
            elif [[ ! -e $base.r ]]; then
                # No expected results?  Write all the output to the sumfile.
                # (It is also always written to the logfile: see below.)
                want_all_output=t
            fi

            if [[ -f core ]] || [[ "$(find $tmpdir -name core -print)" != "" ]]; then
                # A coredump in the current directory or under the tmpdir.
                # Preserve it in the logdir.

                mv core $logdir/$(echo $base | tr '/' '-').core 2>/dev/null || true
                find $tmpdir -name core -type f -print0 | xargs -0r -I'{}' mv --backup=numbered '{}' $logdir/$(echo $base | tr '/' '-').core
                fail=t
                failmsg="core dumped"

            # Expected tag not found.
            elif [[ -n $expected_tag ]]; then
                fail=t
                failmsg="expected tag $expected_tag"
                reinvoke_failure=-1

            # Detect a timeout.
            elif [[ $exitcode -eq $TIMEOUTRET ]] && [[ -z $IGNORE_TIMEOUTS ]] &&
                 [[ "x$(extract_options timeout-success $_test)" = "x" ]]; then
                fail=t
                failmsg="timed out"

            # Exitcode of 67 == XFAIL.
            elif [[ $exitcode -eq 67 ]]; then
                [[ $expected_exitcode -eq 1 ]] && fail=t
                xfail=t
                failmsg="requested by test"

            # Exitcodes are not useful if there's been a coredump, but otherwise...
            elif [[ $exitcode != $expected_exitcode ]] && [[ $exitcode -ne $TIMEOUTRET ]]; then

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

            reinvoke_failure=$((reinvoke_failure-1))
            if [[ -z $fail ]]; then
                # No reinvocation if we didn't fail.
                reinvoke_failure=-1
            elif [[ -n $fail ]] && [[ $reinvoke_failure -ge 0 ]]; then
                reinvoked=$((reinvoked+1))
                testmsg="after $reinvoked reinvocations"

                resultslog
            fi
            dt_flags="$orig_dt_flags"
        done

        # If it xpasses but is unstable, just call it xfail.
        if [[ -z $fail ]] && [[ -n $xfail ]] && [[ "$(extract_options tags $_test)" == *unstable* ]]; then
            fail=t

            if [[ -z $xfailmsg ]]; then
                xfailmsg="xpassed but unstable"
            else
                xfailmsg="xpassed but unstable; $xfailmsg"
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

            # If it fails, is not xfail, and is unstable, just call it xfail.
            if [[ -z $xfail ]] && [[ "$(extract_options tags $_test)" == *unstable* ]]; then
                xfail=t

                if [[ -z $xfailmsg ]]; then
                    xfailmsg="unstable"
                else
                    xfailmsg="unstable; $xfailmsg"
                fi
            fi

            fail "$xfail" "$xfailmsg" "$failmsg${capturing:+, $capturing}"
        fi

        resultslog t

        # If capturing results is requested, capture them now.
        if [[ -n $capturing ]]; then
            cp -f $tmpdir/test.out $base.r
        fi

        test/utils/clean_probes.sh >> $LOGFILE

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
            DTRACE_DEBUG= $dt -S $dt_flags -e > $tmpdir/regtest.out 2>&1 
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

    gawk -f - $SUMFILE <<'EOF' | tee -a $LOGFILE $SUMFILE
{
	rc = 0;
}
match($0, /: X?(FAIL|PASS|SKIP)/) {
	rc = substr($0, RSTART + 2, RLENGTH - 2);
	count[rc]++;
	total++;
}
rc && match($0, /after [0-9]+ reinvocations/) {
	$0 = substr($0, RSTART);
	count["REINVOKES"] += int($2);
}
END {
        if (count["REINVOKES"] == 0)
	    printf("%d cases (%d PASS, %d FAIL, %d XPASS, %d XFAIL, %d SKIP)\n",
		    total, count["PASS"], count["FAIL"], count["XPASS"],
		           count["XFAIL"], count["SKIP"]);
	else
	    printf("%d cases (%d PASS (%d reinvocations), %d FAIL, %d XPASS, %d XFAIL, %d SKIP)\n",
		    total, count["PASS"], count["REINVOKES"], count["FAIL"],
		    count["XPASS"], count["XFAIL"], count["SKIP"]);
}
EOF

fi

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
            gawk 'BEGIN { quiet=1; } { if (!quiet) { print ($0); } } /^Overall coverage rate:$/ { quiet=0; }' | \
            tee -a $LOGFILE $SUMFILE
    fi
done

if [[ -n $ERRORS ]]; then
    exit 1
fi

exit 0
