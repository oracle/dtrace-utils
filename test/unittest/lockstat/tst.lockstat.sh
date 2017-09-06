#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# Test lockstat:::*acquire*, *release probes.
#
#

if (( $# != 1 )); then
	print -u2 "expected one argument: <dtrace-path>"
	exit 2
fi

dtrace=$1

$dtrace $dt_flags -c "sleep 10" -qs /dev/stdin <<EODTRACE
lockstat:::
{
	events[probename]++;
}

END
{
	printf("Minimum lockstat events seen\n\n");
	/*
	 * we sometimes do not see *-spin or *-block events as lock
	 * acquisition often succeeds without spinning or blocking.
	 */
	printf("lockstat:::adaptive-spin - %s\n",
	    events["adaptive-spin"] >= 0 ? "yes" : "no");
	printf("lockstat:::adaptive-block - %s\n",
	    events["adaptive-block"] >= 0 ? "yes" : "no");
	printf("lockstat:::adaptive-acquire - %s\n",
	    events["adaptive-acquire"] > 0 ? "yes" : "no");
	printf("lockstat:::adaptive-release - %s\n",
	    events["adaptive-release"] > 0 ? "yes" : "no");
	printf("lockstat:::rw-spin - %s\n",
	    events["rw-spin"] >= 0 ? "yes" : "no");
	printf("lockstat:::rw-acquire - %s\n",
	    events["rw-acquire"] > 0 ? "yes" : "no");
	printf("lockstat:::rw-release - %s\n",
	    events["rw-release"] > 0 ? "yes" : "no");
	printf("lockstat:::spin-spin - %s\n",
	    events["spin-spin"] >= 0 ? "yes" : "no");
	printf("lockstat:::spin-acquire - %s\n",
	    events["spin-acquire"] > 0 ? "yes" : "no");
	printf("lockstat:::spin-release - %s\n",
	    events["spin-release"] > 0 ? "yes" : "no");
}
EODTRACE
