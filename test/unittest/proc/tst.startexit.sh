#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This script tests that the firing order of probes in a process is:
# 
#  1.  proc:::start
#  2.  proc:::lwp-start
#  3.  proc:::lwp-exit
#  4.  proc:::exit
#
# If this fails, the script will run indefinitely; it relies on the harness
# to time it out.
#
# @@xfail: dtv2

script()
{
	$dtrace $dt_flags -s /dev/stdin <<EOF
	proc:::start
	/ppid == $child/
	{
		self->start = 1;
	}

	proc:::lwp-start
	/self->start/
	{
		self->lwp_start = 1;
	}

	proc:::lwp-exit
	/self->lwp_start/
	{
		self->lwp_exit = 1;
	}

	proc:::exit
	/self->lwp_exit == 1/
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
