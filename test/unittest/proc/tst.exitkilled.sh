#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This script tests that the proc:::exit probe fires with the correct argument
# when the process is killed.

script()
{
	$dtrace $dt_flags -s /dev/stdin <<EOF
	syscall::execve:entry
	/(this->myargs = (uintptr_t *)copyin((uintptr_t)args[1], 2 * sizeof(char *)))
	 && copyinstr(this->myargs[0]) == "sleep"
	 && this->myargs[1]
	 && copyinstr(this->myargs[1]) == "10000"/
	{
		kill_pid = pid;
	}

	proc:::exit
	/curpsinfo->pr_ppid == $child && kill_pid == pid &&
	 args[0] == CLD_KILLED/
	{
		exit(0);
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
		kill -9 $target
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

pkill -P $child
kill $child

exit $status
