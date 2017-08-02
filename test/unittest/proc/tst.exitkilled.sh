#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright © 2007, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This script tests that the proc:::exit probe fires with the correct argument
# when the process is killed.
#
# If this fails, the script will run indefinitely; it relies on the harness
# to time it out.
#
script()
{
	$dtrace $dt_flags -s /dev/stdin <<EOF
	proc:::exit
	/curpsinfo->pr_ppid == $child &&
	    curpsinfo->pr_psargs == "$longsleep" && args[0] == CLD_KILLED/
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
		kill -9 $!
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
