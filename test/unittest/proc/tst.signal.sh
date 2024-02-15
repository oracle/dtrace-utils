#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This script tests that the proc:::signal-send and proc:::signal-handle
# probes fire correctly and with the correct arguments.

script()
{
	$dtrace $dt_flags -s /dev/stdin <<EOF
	syscall::execve:entry
	/(this->myargs = (uintptr_t *)copyin((uintptr_t)args[1], 2 * sizeof(char *)))
	 && copyinstr(this->myargs[0]) == "sleep"
	 && this->myargs[1]
	 && copyinstr(this->myargs[1]) == "10000"/
	{
		sig_pid = pid;
	}

	proc:::signal-send
	/execname == "kill" && curpsinfo->pr_ppid == $child &&
	 sig_pid == args[1]->pr_pid && args[2] != SIGUSR1/
	{
		/* Wrong signal being sent. */
		printf("wrong signal sent: %d vs %d\n", args[2], SIGUSR1);
		exit(1);
	}

	proc:::signal-handle
	/sig_pid == pid/
	{
		printf("signal received %d\n", args[0]);
		exit(args[0] == SIGUSR1 ? 0 : 1);
	}

	tick-5s
	{
		exit(124);
	}
EOF
}

sleeper()
{
	while true; do
		$longsleep &
		target=$!
		disown %+
		sleep 1
		/bin/kill -USR1 $target
		sleep 1
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
