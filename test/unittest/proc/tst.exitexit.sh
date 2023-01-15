#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This script tests that the proc:::exit probe fires with the correct argument
# when the process explicitly exits.
#
# If this fails, the script will run indefinitely; it relies on the harness
# to time it out.

script()
{
	$dtrace $dt_flags -s /dev/stdin <<EOF
	proc:::exit
	/ppid == $child && args[0] == CLD_EXITED/
	{
		exit(0);
	}
EOF
}

sleeper()
{
	while true; do
		sleep 1
	done
}

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

sleeper &
child=$!
disown %+

script
status=$?

kill $child
exit $status
