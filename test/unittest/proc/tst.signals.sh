#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2014, 2021, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# This script tests dtrace's handling of processes that receive various
# signals.
#

# It takes quite a while: up to five seconds per test, plus init overhead,
# times 31 signals, plus signal-determination overhead.
#
# @@timeout: 200
# @@tags: unstable

dtrace=$1

# Coredump to names that we can distinguish from each other: don't
# suppress coredumps.

orig_core_pattern="$(cat /proc/sys/kernel/core_pattern)"
echo core.%e > /proc/sys/kernel/core_pattern
orig_core_uses_pid="$(cat /proc/sys/kernel/core_uses_pid)"
echo 0 > /proc/sys/kernel/core_uses_pid
ulimit -c unlimited

# First, figure out what the test process does when hit by signals.

termsigs=
dumpsigs=
stopsigs=
contsigs=
ignsigs=

for signal in $(seq 1 31); do
    test/triggers/longsleep &
    sleeppid=$!
    disown %+
    sleep .1 # long enough for the exec of sleep(1)
    kill -$signal $sleeppid 2>/dev/null
    sleep .1 # let it hit

    if ! kill -0 $sleeppid 2>/dev/null; then
        # Gone. Terminating signal.

        if [[ -e core.sleep ]]; then
            dumpsigs="$dumpsigs $signal"
            rm -f core.sleep
        else
            termsigs="$termsigs $signal"
        fi
    elif [[ "x$(ps -o state= -p $sleeppid)" == "xT" ]]; then
        # Stopping signal.  Force-kill it: we'll look for
        # contsigs later.  We can rely on -9 being SIGKILL.
        kill -9 $sleeppid 2>/dev/null;
        stopsigs="$stopsigs $signal"
    else
        # Provisional, may include stopsigs.  Will also include any
        # signals which sleep(1) masks.
        kill -9 $sleeppid 2>/dev/null
        ignsigs="$ignsigs $signal"
    fi
done

# Now hunt for SIGCONT.
for signal in $ignsigs; do
    test/triggers/longsleep &
    sleeppid=$!
    disown %+
    sleep .1 # long enough for the exec of sleep(1)
    kill -$(echo $stopsigs | tr " " "\n" | head -1) $sleeppid 2>/dev/null
    sleep .1 # let it hit
    kill -$(echo $signal | tr " " "\n" | head -1) $sleeppid 2>/dev/null
    sleep .1 # let it hit

    if ! kill -0 $sleeppid 2>/dev/null; then
        # This time it killed us. Strange. Ignore it.
        :
    elif [[ "x$(ps -o state= -p $sleeppid)" != "xT" ]]; then
        # SIGCONT.
        contsigs="$contsigs|$signal"
        ignsigs="$(echo "$ignsigs" | tr " " "\n" | grep -v $signal)"
        kill -9 $sleeppid 2>/dev/null
    fi
done

# Transform the signal sets into egrep alternations.
termsigs="^($(echo $termsigs | tr ' ' '|'))\$"
dumpsigs="^($(echo $dumpsigs | tr ' ' '|'))\$"
stopsigs="^($(echo $stopsigs | tr ' ' '|'))\$"
contsigs="^($(echo $contsigs | tr ' ' '|'))\$"
ignsigs="^($(echo $ignsigs | tr ' ' '|'))\$"

# Now run dtrace over sleep repeatedly, hitting it with each signal in turn and
# watching its debugging output for the appropriate result.

DIRNAME="$tmpdir/libproc-tst-signals-sh.$$.$RANDOM"
mkdir -p $DIRNAME

for signal in $(seq 1 31); do
    test/triggers/longsleep &
    proc_pid=$!

    DTRACE_DEBUG=t $dtrace $dt_flags -xquiet -p $! \
                -n 'tick-5s { exit(0); }' \
                2> $DIRNAME/dtrace.out &
    dt_pid=$!

    while ! grep -q ': Handling a dt_proc_continue()' $DIRNAME/dtrace.out; do
        sleep .25
    done

    kill -$signal $proc_pid >/dev/null 2>&1
    sleep 1
    if echo $signal | grep -qE $termsigs; then
        grep -Fq "child got terminating signal $signal." $DIRNAME/dtrace.out ||
            { echo "Terminating signal $signal not handled as expected." >&2;
              cat $DIRNAME/dtrace.out >&2; }
    elif echo $signal | grep -qE $stopsigs; then
        grep -Fq "child got stopping signal $signal." $DIRNAME/dtrace.out ||
            { echo "Stopping signal $signal not handled as expected." >&2
              cat $DIRNAME/dtrace.out >&2; }
    elif echo $signal | grep -qE $ignsigs; then
        grep -vq 'child got .*signal' $DIRNAME/dtrace.out ||
            { echo "Ignored signal $signal not handled as expected." >&2
              cat $DIRNAME/dtrace.out >&2; }
    elif echo $signal | grep -qE $dumpsigs; then
        { grep -vq 'child got terminating signal' $DIRNAME/dtrace.out &&
            [[ -e core.sleep ]]; } ||
            { echo "Coredump signal $signal not handled as expected or no coredump found." >&2
              cat $DIRNAME/dtrace.out >&2; }
        rm -f core.sleep
    elif echo $signal | grep -qE $stopsigs; then
        grep -q "Process got SIGCONT" $DIRNAME/dtrace.out ||
            echo "SIGCONT not handled as expected." >&2
    fi
    kill -9 $proc_pid $dt_pid >/dev/null 2>&1 # Clean up any wreckage
done

echo $orig_core_pattern > /proc/sys/kernel/core_pattern
echo $orig_core_uses_pid > /proc/sys/kernel/core_uses_pid
