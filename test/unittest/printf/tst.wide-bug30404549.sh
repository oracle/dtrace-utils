#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# 30404549 DTrace correct the snprintf maximum lengths in libdtrace/dt_printf.c
#

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi
dtrace=$1

# abbreviate output to a simple statement of length.  raw output is big (29M)
$dtrace $dt_flags -qs /dev/stdin << EOF | gawk '{ print length($0); }'
BEGIN
{
	printf("%10000000d\n", 1);
	printf("%.10000000d\n", 1);
	printf("%10000000.10000000d\n", 1);
	exit(0);
}
EOF
