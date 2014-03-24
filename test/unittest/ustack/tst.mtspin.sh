#!/bin/bash
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
# Copyright 2006 Oracle, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"%Z%%M%	%I%	%E% SMI"

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
