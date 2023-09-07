#!/bin/bash
# 
# Oracle Linux DTrace.
# Copyright (c) 2009, 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# This script verifies that we can fire a probe on each CPU.

# Why isn't this more stable?  Some CPUs can take a long time to fire.
# Allow some reinvokes and let the test run more than a few seconds.
# @@reinvoke-failure: 1

dtrace=$1

DIRNAME="$tmpdir/cpc-allcpus.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

$dtrace $dt_flags -qn '
cpc:::cpu_clock-all-1000000000
/cpus[cpu] != 1/
{
	cpus[cpu] = 1;
	printf("%d\n", cpu);
}

tick-2s
{
	exit(0);
}' | awk 'NF != 0 {print}' | sort > dtrace.out

awk '/^processor/ {print $3}' /proc/cpuinfo | sort > cpuinfo.out

if ! diff -q dtrace.out cpuinfo.out > /dev/null; then
	echo dtrace output
	cat dtrace.out
	echo cpuinfo list
	cat cpuinfo.out
	echo ERROR: difference
	exit 1
fi

echo cpulist `cat dtrace.out`
echo success
exit 0
