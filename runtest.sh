#!/bin/bash
#
# runtest.sh -- A dtrace tester and regression tester.
#
#               Runs a bunch of .d scripts and tests their results.
#               If more than dtrace build directory is available,
#               runs them each in turn, comparing their output
#               and generated intermediate representation.
#

# Sanitize the shell.

shopt -s nullglob
unset CDPATH

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

pid=`echo $$`
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

# Loop over each dtrace test. Non-regression-tests print all output:
# regression tests only print output when something is different.

for tst in $(echo demo/*/*.d); do
    for dt in $dtrace; do
        dt_flags="-e -s $tst"

        case "$tst" in
            ./demo/buf/ring.d) ;&
            ./demo/sched/whererun.d) dt_flags="$dt_flags /bin/date";;

            ./demo/user/badopen.d) ;&
            ./demo/struct/rwinfo.d) ;&
            ./demo/script/tracewrite.d) ;&
            ./demo/profile/profpri.d) ;&
            ./demo/intro/rwtime.d) ;&
            ./demo/intro/trussrw.d) dt_flags="$dt_flags $pid";;

            ./demo/user/errorpath.d) dt_flags="$dt_flags $pid _chdir";;

            ./demo/sched/pritime.d) dt_flags="$dt_flags $pid 1";;

            ./demo/user/userfunc.d) dt_flags="$dt_flags $pid execute";;

            ./demo/struct/kstat.d) dt_flags="-q -e -s $tst $(pgrep mpstat)";;

            ./demo/user/libc.d) dt_flags="$dt_flags -c /usr/bin/date";;
        esac

        if [[ -z $regression ]]; then
            # Non-regression-tests just print the output.

            echo $dt $dt_flags
            $dt $dt_flags
        else
            # Regression tests run with intermediate results displayed, and
            # output redirected to a per-test, per-dtrace temporary file.

            dtest_dir="$tmpdir/$(basename $(dirname $dt))"
            logfile="$(echo $tst | sed 's,/,-,g').log"
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

if [[ -z $regression ]]; then
    #Test grep $pid
    echo $dtrace -e -p $pid
    $dtrace -e -p $pid
else
    # Regtest comparison. Skip one directory and diff all the others
    # against it.

    for name in $tmpdir/*; do
        [[ $name = $first_dtest_dir ]] && continue

        diff -urN $first_dtest_dir $name
    done
fi
