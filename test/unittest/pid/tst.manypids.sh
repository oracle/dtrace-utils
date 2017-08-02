#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright © 2007, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# xfail: needs porting

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

declare -a pids

for lib in `ls -1 /lib/lib*.so.1 | grep -v ld.so.1`; do
	preload=$lib:${preload}
done

export LD_PRELOAD=$preload

let numkids=100
let i=0

tmpfile=/tmp/dtest.$$

while [ "$i" -lt "$numkids" ]; do
	sleep 500 &
	pids[$i]=$!
        disown %+
	let i=i+1
done

export LD_PRELOAD=

let i=0

echo "tick-1sec { exit(0); }" > $tmpfile

while [ "$i" -lt "$numkids" ]; do
	echo "pid${pids[$i]}::malloc:entry {}" >> $tmpfile
	let i=i+1
done

$dtrace $dt_flags -s $tmpfile
status=$?

rm $tmpfile
pkill sleep -P $$
exit $status
