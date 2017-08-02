#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
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
# matches as well as the full module name. We'll use 'libc.so.1'
# (and therefore 'libc' and 'libc.so') as it's definitely there.
#

for lib in libc libc.so libc.so.1; do
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
