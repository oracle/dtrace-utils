#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This script tests that the proc:::signal-send and proc:::signal-handle
# probes fire correctly and with the correct arguments.
#
# If this fails, the script will run indefinitely; it relies on the harness
# to time it out.
#
script()
{
	$dtrace $dt_flags -s /dev/stdin <<EOF
	proc:::signal-send
	/execname == "kill" && curpsinfo->pr_ppid == $child &&
	    args[1]->pr_psargs == "$longsleep" && args[2] == SIGUSR1/
	{
		/*
		 * This is guaranteed to not race with signal-handle.
		 */
		target = args[1]->pr_pid;
	}

	proc:::signal-handle
	/target == pid && args[0] == SIGUSR1/
	{
		exit(0);
	}
EOF
}

sleeper()
{
	while true; do
		$longsleep &
		disown %+
		sleep 1
		/bin/kill -USR1 $!
	done
}

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
longsleep="sleep 10000"

sleeper &
child=$!
disown %+

script
status=$?

kill -STOP $child
pkill -P $child
kill $child
kill -CONT $child

exit $status
