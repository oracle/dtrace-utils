#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This test verifies that DTrace discovers and processes USDT probes in a
# process that gets executed afer the DTrace session has started (-Z is
# required).

dtrace=$1
trigger=`pwd`/test/triggers/usdt-tst-defer

# Set up test directory.
DIRNAME=$tmpdir/defer-Z-basic.$$.$RANDOM
mkdir -p $DIRNAME
cd $DIRNAME

# Make a private copy of the trigger so that we get our own DOF stash.
cp $trigger main

# Start dtrace.
$dtrace $dt_flags -Zq -o dtrace.out -n '
testprov*:::foo,
testprov*:::bar
{
	printf("%s:%s %d\n", probemod, probename, pid);
}' &
dtpid=$!

# Wait up to half of the timeout period for dtrace to start up.
iter=$((timeout / 2))
while [ $iter -gt 0 ]; do
	sleep 1
	if [ -e dtrace.out ]; then
		break
	fi
	iter=$((iter - 1))
done
if [[ $iter -eq 0 ]]; then
	echo ERROR starting DTrace job
	cat dtrace.out
	exit 1
fi

# Start trigger process.
./main > main.out &
tpid=$!

# Confirm that dtrace is still running (otherwise trigger runs forever).
sleep 2
if [[ ! -d /proc/$dtpid ]]; then
	echo ERROR dtrace died after trigger started
	kill -USR1 $tpid
	wait $tpid
	exit 1
fi

# Wait for process to complete.
wait $tpid

# Kill the dtrace process.
kill $dtpid
wait

# Check the program output (main.out).
echo "$tpid: undefined 1 0 10 10 10" > main.out.expected
awk '{ $2 = "undefined"; print }' main.out > main.out.post
if ! diff -q main.out.post main.out.expected; then
	echo program output looks wrong
	echo === was ===
	cat main.out
	echo === got ===
	cat main.out.post
	echo === expected ===
	cat main.out.expected
	exit 1
fi

# Regularize the DTrace output, and check it.
awk 'NF > 0 { map[$2 " " $1]++; }
     END { for (i in map) printf "%s %d\n", i, map[i]; }' dtrace.out > dtrace.out.post

echo "$tpid main:bar 10" > dtrace.out.expected

if ! sort dtrace.out.expected | diff -q - dtrace.out.post; then
	echo dtrace output looks wrong
	echo === was ===
	cat dtrace.out
	echo === got ===
	cat dtrace.out.post
	echo === expected ===
	sort dtrace.out.expected
	echo === diff ===
	sort dtrace.out.expected | diff - dtrace.out.post
	exit 1
fi

echo success

exit 0
