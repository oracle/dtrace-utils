#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi
# @@xfail: dtv2

dtrace=$1

# The output files assumes the timezone is US/Pacific
export TZ=US/Pacific

$dtrace $dt_flags -s /dev/stdin <<EOF
#pragma D option quiet

inline uint64_t NANOSEC = 1000000000;

BEGIN
{
	printf("%Y\n%Y\n%Y", (uint64_t)0, (uint64_t)1062609821 * NANOSEC,
	    (uint64_t)0x7fffffff * NANOSEC);
	exit(0);
}
EOF

if [ $? -ne 0 ]; then
	print -u2 "dtrace failed"
	exit 1
fi

exit 0
