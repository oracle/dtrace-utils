#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

declare -a pids

for lib in `ls -1 /lib64/lib*.so.1 | grep -v libthread_db.so.1`; do
	preload=$lib:${preload}
done

export LD_PRELOAD=$preload
# Force libraries and executable to get pulled into the cache so that starting
# the disowned processes in the loop below will go fast enough that by the time
# dtrace is started, all tasks are ready to be traced.
/usr/bin/sleep 0

let numkids=100
let i=0

tmpfile=$tmpdir/dtest.$$

while [ "$i" -lt "$numkids" ]; do
	/usr/bin/sleep 500 &
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
