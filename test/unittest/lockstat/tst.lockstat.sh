#!/bin/bash

#
# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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
	 * we sometimes do not see adaptive-acquire-starts because
	 * trylock for mutex often succeeds.
	 */
	printf("lockstat:::adaptive-acquire-start - %s\n",
	    events["adaptive-acquire-start"] >= 0 ? "yes" : "no");
	printf("lockstat:::adaptive-acquire - %s\n",
	    events["adaptive-acquire"] > 0 ? "yes" : "no");
	printf("lockstat:::adaptive-release - %s\n",
	    events["adaptive-release"] > 0 ? "yes" : "no");
	printf("lockstat:::rw-acquire-start - %s\n",
	    events["rw-acquire-start"] > 0 ? "yes" : "no");
	printf("lockstat:::rw-acquire - %s\n",
	    events["rw-acquire"] > 0 ? "yes" : "no");
	printf("lockstat:::rw-release - %s\n",
	    events["rw-release"] > 0 ? "yes" : "no");
	printf("lockstat:::spinlock-acquire-start - %s\n",
	    events["spinlock-acquire-start"] > 0 ? "yes" : "no");
	printf("lockstat:::spinlock-acquire - %s\n",
	    events["spinlock-acquire"] > 0 ? "yes" : "no");
	printf("lockstat:::spinlock-release - %s\n",
	    events["spinlock-release"] > 0 ? "yes" : "no");
}
EODTRACE
