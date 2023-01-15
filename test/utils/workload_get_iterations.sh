#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# Get the number of iterations needed for the target program ($1)
# to run in the specified number of seconds ($2).

# The target program, in test/utils/, which takes some iteration count
# as a command-line argument.
prog=`dirname $0`/$1
if [ ! -e $prog ]; then
	echo -2
	exit 1
fi

# The target number of seconds we want the program to run.
nsecs=$2

# For the target program and number of seconds, return an estimate of how
# many iterations are needed.

function time_me() {
	# FIXME: exit this script if we encounter an error
	/usr/bin/time -f "%e" $prog $1 2>&1
}

# Confirm that one iteration is not enough.
t=$(time_me 1)
if [ `echo "$t >= 1" | bc` -eq 1 ]; then
	echo -1
	exit 1
fi

# Increase estimate exponentially until we we are in range.
n=1
while [ `echo "$t < 0.1 * $nsecs" | bc` -eq 1 ]; do

	# protect against run-away values
	if [ `echo "$n > 30000000000" | bc` -eq 1 ]; then
		echo -$n
		exit 1
	fi

	# increase n
	n=`echo "$n * 10" | bc`
	t=$(time_me $n)
done

# At this point, we should be close enough that we can extrapolate.
echo "$nsecs * $n / $t" | bc
exit 0
