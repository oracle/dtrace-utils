#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

dtrace=$1
tmpfile=$tmpdir/tst.list_cpc.$$
mkdir $tmpfile
cd $tmpfile

$dtrace $dt_flags -lP cpc > out.txt 2> err.txt
if [ $? -ne 0 ]; then
	echo "DTrace error"
	echo "==== out.txt"
	cat out.txt
	echo "==== err.txt"
	cat err.txt
	exit 1
fi

awk '
BEGIN { cpu_clock = task_clock = 0 }
$2 == "cpc" && index($3, "perf_count_sw_cpu_clock-") { cpu_clock = 1; next }
$2 == "cpc" && index($3, "perf_count_sw_task_clock-") { task_clock = 1; next }
END {
	if (cpu_clock && task_clock) exit(0);
	if (cpu_clock == 0) { print "missing CPU clock" }
	if (task_clock == 0) { print "missing task clock" }
	exit(1);
}' out.txt

if [ $? -ne 0 ]; then
	echo "unexpected results"
	echo "==== out.txt"
	cat out.txt
	echo "==== err.txt"
	cat err.txt
	exit 1
fi

exit 0
