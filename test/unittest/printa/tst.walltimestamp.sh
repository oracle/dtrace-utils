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

dtrace=$1

# The output file assumes the timezone is US/Pacific
export TZ=US/Pacific

$dtrace $dt_flags -s /dev/stdin <<EOF
#pragma D option quiet
#pragma D option destructive

BEGIN
{
	@foo = min(1075064400 * 1000000000);
	@bar = max(walltimestamp);
	printa("%@T\n", @foo);
	printa("%@Y\n", @foo);

	freopen("/dev/null");
	printa("%@T\n", @bar);
	printa("%@Y\n", @bar);

	exit(0);
}
EOF

if [ $? -ne 0 ]; then
	echo "dtrace failed" >&2
	exit 1
fi

exit 0
