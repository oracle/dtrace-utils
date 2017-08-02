#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright © 2007, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@skip: leaves a ping hanging, causing interference

#
# This script tests that several of the the mib:::icmp* probes fire and fire
# with a valid args[0].
#
script()
{
	$dtrace $dt_flags -s /dev/stdin <<EOF
	mib:::icmpInEchos
	{
		in = args[0];
	}

	mib:::icmpOutEchoReps
	{
		reps = args[0];
	}

	mib:::icmpOutMsgs
	{
		msgs = args[0];
	}

	profile:::tick-10msec
	/in && reps && msgs/
	{
		exit(0);
	}
EOF
}

pinger()
{
	ping localhost
}

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

pinger &
pinger=$!
disown %+
script
status=$?

kill $pinger
exit $status
