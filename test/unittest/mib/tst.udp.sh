#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This script tests that several of the the mib:::udp* probes fire and fire
# with a valid args[0].
#
script()
{
	$dtrace $dt_flags -s /dev/stdin <<EOF
	mib:::udpHCOutDatagrams
	{
		out = args[0];
	}

	mib:::udpHCInDatagrams
	{
		in = args[0];
	}

	profile:::tick-10msec
	/in && out/
	{
		exit(0);
	}
EOF
}

rupper()
{
	while true; do
		rup localhost || exit 1
		sleep 1
	done
}

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

rupper &
rupper=$!
disown %+
script
status=$?

kill $rupper
exit $status
