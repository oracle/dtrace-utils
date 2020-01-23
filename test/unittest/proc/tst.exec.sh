#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This script tests that the proc:::exec probe fires, followed by the
# proc:::exec-success probe (in a successful exec(2)).
#
# If this fails, the script will run indefinitely; it relies on the harness
# to time it out.
#
# @@xfail: dtv2

script()
{
	$dtrace $dt_flags -s /dev/stdin <<EOF
	proc:::exec
	/ppid == $child && basename(args[0]) == "sleep"/
	{
		self->exec = 1;
	}

	proc:::exec-success
	/self->exec/
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
