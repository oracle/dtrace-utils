#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

#
# Let's see if we can successfully specify a module using partial
# matches as well as the full module name. We'll use 'libc.so.<n>'
# (and therefore 'libc' and 'libc.so') as it's definitely there.
#
# First we need to determine the basename of the linked libc for
# sleep(1)
#
names=`ldd /usr/bin/sleep | \
	awk '/libc.so/ {
		n = split($1, a, /\./);
		l = a[1];
		s = l;
		for (i = 2; i <= n; i++) {
			s = s "." a[i];
			l = l " " s;
		}
		print l;
	    }'`

for lib in $names; do
	sleep 60 &
	pid=$!
	disown %+
	$dtrace $dt_flags -n "pid$pid:$lib::entry" -n 'tick-2s{exit(0);}'
	status=$?

	kill $pid

	if [ $status -gt 0 ]; then
		exit $status
	fi
done

exit $status
