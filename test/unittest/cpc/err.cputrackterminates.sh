#!/bin/bash
# 
# Oracle Linux DTrace.
# Copyright (c) 2009, 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

#
# This script ensures that cputrack(1) will terminate when the cpc provider
# kicks into life.
#
# The script will fail if:
#	1) The system under test does not define the 'PAPI_tot_ins' event.

# @@skip: no cputrack on Linux, and should switch PAPI_tot_ins to cpu-clock

script()
{
	$dtrace $dt_flags -s /dev/stdin <<EOF
	#pragma D option bufsize=128k

	cpc:::PAPI_tot_ins-all-10000
	{
		@[probename] = count();
	}

	tick-1s
	/n++ > 10/
	{
		exit(0);
	}
EOF
}

if [ $# != 1 ]; then
        echo expected one argument: '<'dtrace-path'>'
        exit 2
fi

dtrace=$1

cputrack -c PAPI_tot_ins sleep 20 &
cputrack_pid=$!
sleep 5
script 2>/dev/null &

wait $cputrack_pid
status=$?

rm $dtraceout

exit $status
