#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This script tests that the proc:::signal-discard probe fires correctly
# and with the correct arguments.
#
# If this fails, the script will run indefinitely; it relies on the harness
# to time it out.
#
script()
{
	$dtrace $dt_flags -s /dev/stdin <<EOF
	proc:::signal-discard
	/args[1]->pr_pid == $child &&
	    args[1]->pr_psargs == "$longsleep" && args[2] == SIGHUP/
	{
		exit(0);
	}
EOF
}

killer()
{
	while true; do
		sleep 1
		kill -HUP $child
	done
}

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
longsleep="sleep 10000"

/usr/bin/nohup $longsleep &
child=$!
disown %+

killer &
killer=$!
disown %+
script
status=$?

kill $child
kill $killer

exit $status
