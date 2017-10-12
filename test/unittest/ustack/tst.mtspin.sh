#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
# @@skip: unreliable, see bug 22269741

# @@tags: unstable

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

file=/tmp/out.$$
dtrace=$1

rm -f $file

$dtrace $dt_flags -o $file -c test/triggers/ustack-tst-mtspin -s /dev/stdin <<EOF

	#pragma D option quiet
	#pragma D option destructive
	#pragma D option evaltime=main

	/*
	 * Toss out the first 100 samples to wait for the program to enter
	 * its steady state.
	 */

	profile-1999
	/pid == \$target && n++ > 100/
	{
		@total = count();
		@stacks[ustack()] = count();
	}

	tick-1s
	{
		secs++;
	}

	tick-1s
	/secs > 5/
	{
		done = 1;
	}

	tick-1s
	/secs > 10/
	{
		trace("test timed out");
		exit(1);
	}

	profile-1999
	/pid == \$target && done/
	{
		raise(SIGINT);
		exit(0);
	}

	BEGIN
	{
		printf("%i\n", \$target);
	}

	END
	{
		printa("TOTAL %@u\n", @total);
		printa("START%kEND\n", @stacks);
	}
EOF

status=$?
cat $file
if [ "$status" -ne 0 ]; then
	echo $tst: dtrace failed
	rm -f $file
	exit $status
fi

if ps -p $(head $file) >/dev/null 2>&1; then
	echo "PID $(head $file) is still alive: was probably crashed by ustack()." >&2;
	kill -9 $(head $file)
	exit 1;
fi

rm -f $file

exit $status
