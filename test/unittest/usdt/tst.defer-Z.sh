#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This test verifies that USDT will see new processes, even if detection
# is deferred -- that is, DTrace does not know about a new USDT process
# until after it's started running.
#
# In this test, all processes are started after the DTrace session has started.
# So the USDT probes will not be recognized at first and -Z must be used.

dtrace=$1
trigger=`pwd`/test/triggers/usdt-tst-defer

# Set up test directory.

DIRNAME=$tmpdir/defer-Z.$$.$RANDOM
mkdir -p $DIRNAME
cd $DIRNAME

# Make a private copy of the trigger executable so that we get our
# own DOF stash.

cp $trigger main

# Start dtrace.

$dtrace $dt_flags -Zwq -o dtrace.out -n '
BEGIN
{
	printf("BEGIN\n");
}
testprov*:::foo
{
	raise(SIGUSR1);
}
testprov*0:::bar,
testprov*1:::bar,
testprov*2:::bar,
testprov*3:::bar,
testprov*4:::bar
{
	@[pid, 0] = sum(arg0);
	@[pid, 1] = sum(arg1);
	@[pid, 2] = sum(arg2);
	@[pid, 3] = sum(pid % 100);
}' &
dtpid=$!

# Wait up to half of the timeout period for dtrace to start up.

iter=$((timeout / 2))
while [ $iter -gt 0 ]; do
	sleep 1
	if [ -s dtrace.out ]; then
		break
	fi
	iter=$((iter - 1))
done
if [[ $iter -eq 0 ]]; then
	echo ERROR starting DTrace job
	cat dtrace.out
	exit 1
fi

# Start processes concurrently.

num=10
i=0
while [ $i -lt $num ]; do
	./main > main.out$i &
	pids[$i]=$!
	i=$(($i + 1))
done

# Confirm that dtrace is still running (otherwise triggers run forever).
sleep 2
if [[ ! -d /proc/$dtpid ]]; then
	echo ERROR dtrace died after triggers started
	i=0
	while [ $i -lt $num ]; do
		kill -USR1 ${pids[$i]}
		wait       ${pids[$i]}
		i=$(($i + 1))
	done
	exit 1
fi

# Wait for processes to complete.

i=0
while [ $i -lt $num ]; do
	wait ${pids[$i]}
	i=$(($i + 1))
done

# Kill the dtrace process.

kill $dtpid
wait

# Check the program output (main.out$i files).

i=0
while [ $i -lt $num ]; do
	if [ $((${pids[$i]} % 10)) -lt 5 ]; then
		nphase2bar=10
	else
		nphase2bar=0
	fi
	echo "${pids[$i]}: undefined 0 0 10 10 $nphase2bar" > main.out$i.expected
	awk '
	    $3 == "1" { $3 =   0 }   # in phase 1, round 1 down to 0
	    $4 == "1" { $4 =   0 }   # in phase 1, round 1 down to 0
	    { $2 = "undefined"; print }' main.out$i > main.out$i.post
	if ! diff -q main.out$i.post main.out$i.expected; then
		echo program output looks wrong for DTrace case $i
		echo === was ===
		cat main.out$i
		echo === got ===
		cat main.out$i.post
		echo === expected ===
		cat main.out$i.expected
		exit 1
	fi
	i=$(($i + 1))
done

# Check the dtrace output.

#     regularize the dtrace output
awk 'NF == 3 { print $1, $2, $3 }' dtrace.out | sort > dtrace.out.post

#     determine what to expect

i=0
while [ $i -lt $num ]; do
	if [ $((${pids[$i]} % 10)) -lt 5 ]; then
		x=$(((${pids[$i]} % 100) * 10))
		echo ${pids[$i]} 0 45 >> dtrace.out.expected
		echo ${pids[$i]} 1 65 >> dtrace.out.expected
		echo ${pids[$i]} 2 90 >> dtrace.out.expected
		echo ${pids[$i]} 3 $x >> dtrace.out.expected
	fi

	i=$(($i + 1))
done

#     diff
if ! sort dtrace.out.expected | diff -q - dtrace.out.post; then
	echo dtrace output looks wrong for DTrace case $i
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
