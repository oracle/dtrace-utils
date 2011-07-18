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

[[ -f ./runtest.conf ]] && . ./runtest.conf

#
# get_dir_name
#
# Pick a unique temporary directory name to stick things in.
# Returns results via `echo'; must be called via substitution.
#
# Respect TMPDIR if set, as POSIX requires.

get_dir_name()
{
    typeset tmpname;                  # A subdir name being tested for uniqueness

    false;                            # Starting state - reset $?
    while [[ $? -ne 0 ]]; do
        tmpname=${TMPDIR:-/tmp}/runtest.${RANDOM}
        mkdir $tmpname                # Test-and-set it
    done

    echo $tmpname                     # Name is unique, return it
}

# Make a temporary directory for intermediate result storage.

typeset tmpdir=$(get_dir_name)
trap 'cd; rm -rf ${tmpdir}; exit' INT QUIT TERM SEGV EXIT

# Variables usable in demo flags.

# TODO: allow specification of a program to auto-launch and later
# attach to, instead of using our own shell's PID. Allow
# overriding of -e, so as to test things with execution (but
# this requires some way to force the execution to terminate:
# perhaps time-based via wait(1)?).

_pid=`echo $$`
_exit="-e"

dtrace="./build-*/dtrace"
if [[ -z $dtrace ]]; then
	echo "No dtraces available.";
	exit 1;
fi

# More than one dtrace tree -> verify identical results for each dtrace.

regression=
first_dtest_dir=
if [[ $(echo $dtrace | wc -w) -gt 1 ]]; then
    regression=t
fi

# Initialize test coverage.

for name in build-*; do
    if [[ -n "$(echo $name/*.gcno)" ]]; then
        rm -rf $name/coverage
        mkdir -p $name/coverage
        lcov --capture --base-directory . --directory $name --initial \
             --quiet -o $name/coverage/initial.lcov
    fi
done

if [[ -n $KERNEL_BUILD_DIR ]] && [[ -d $KERNEL_BUILD_DIR ]] &&
   [[ -d /sys/kernel/debug/gcov/$KERNEL_BUILD_DIR/kernel/dtrace ]]; then
        rm -rf $KERNEL_BUILD_DIR/coverage
        mkdir -p $KERNEL_BUILD_DIR/coverage
        lcov --capture --base-directory $KERNEL_BUILD_DIR --initial \
             --quiet -o $KERNEL_BUILD_DIR/coverage/initial.lcov
fi

# Loop over each dtrace test. Non-regression-tests print all output:
# regression tests only print output when something is different.

for _test in $(find test -name "*.d"); do
    for dt in $dtrace; do
        dt_flags="$_exit -s $_test"

        # Find embedded commands.

        if grep -q @@runtest- $_test; then

            # Optionally skip this test.

            if grep -Eq '@@runtest-skip' $_test; then
                continue
            fi

            # Substitute in flags. The horrendous echo '' here is because
            # printf squashes out spaces, echo -e -e foo prints 'foo',
            # and echo -e -- "foo" prints "-- foo".

            if grep -Eq '@@runtest-opts:|@@runtest-opts-override:' $_test; then
                opts="$(grep -E '@@runtest-opts:|@@runtest-opts-override:' $_test | \
                        sed 's,^.*runtest-opts[^:]*: ,,; s,\*/$,,')"
                override="$(grep '@@runtest-opts-override:' $_test)"

                # If runtest-opts is used, rather than runtest-opts-override,
                # the flags go after the dt_flags above. Otherwise, they
                # replace them completely: the runtest-opts-override
                # must use $_test to specify the test to run, and $_exit
                # to specify whether to exit before execution.
                if [[ -n $opts ]] && [[ -z $override ]]; then
                    dt_flags="$dt_flags $(eval echo '' "$opts")"
                else
                    dt_flags="$(eval echo '' "$opts")"
                fi
            fi
        fi

        if [[ -z $regression ]]; then
            # Non-regression-tests just print the output.

            echo $dt $dt_flags
            $dt $dt_flags
        else
            # Regression tests run with intermediate results displayed, and
            # output redirected to a per-test, per-dtrace temporary file.

            dtest_dir="$tmpdir/$(basename $(dirname $dt))"
            logfile="$(echo $_test | sed 's,/,-,g').log"
            mkdir -p $dtest_dir
            echo $dt_flags > $dtest_dir/$logfile
            echo >> $dtest_dir/$logfile
            $dt -S $dt_flags 2>&1 | sed 's,0x[0-9a-f][0-9a-f]*,{ptr},g' >> $dtest_dir/$logfile

            if [[ -z $first_dtest_dir ]]; then
                first_dtest_dir=$dtest_dir
            fi

        fi
    done
done

if [[ -n $regression ]]; then
    # Regtest comparison. Skip one directory and diff all the others
    # against it.

    for name in $tmpdir/*; do
        [[ $name = $first_dtest_dir ]] && continue

        diff -urN $first_dtest_dir $name
    done
fi

# Test coverage.
for name in build-*; do
    if [[ -n "$(echo $name/*.gcda)" ]]; then
        echo "Coverage info for $name:"
        lcov --capture --base-directory . --directory $name \
             --quiet -o $name/coverage/runtest.lcov
        lcov --add-tracefile $name/coverage/initial.lcov \
             --add-tracefile $name/coverage/runtest.lcov \
             --quiet -o $name/coverage/coverage.lcov
        genhtml --frames --show-details -o $name/coverage \
                --title "DTrace userspace coverage" \
                --highlight --legend $name/coverage/coverage.lcov | \
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
