#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

# Pick a CPU at random.
cpulist=( `awk '/^processor[ 	]*: [0-9]*$/ {print $3}' /proc/cpuinfo` )
ncpus=${#cpulist[@]}
cpu0=${cpulist[$((RANDOM % $ncpus))]}

# Observe where DTrace runs.
cpu=`$dtrace $dt_flags -xcpu=$cpu0 -qn 'tick-100ms { trace(cpu); exit(0); }'`

# Check result.
echo expected cpu $cpu0 got cpu $cpu
if [ `echo $cpu | wc -w` -ne 1 ]; then
	exit 1
elif [ $(($cpu + 0)) != $cpu0 ]; then
	exit 1
fi

exit 0
