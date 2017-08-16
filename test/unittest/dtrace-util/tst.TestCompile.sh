#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
#
# ASSERTION:
# Using the -e option exits after compiling any requests but before
# enabling probes.
#
# SECTION: dtrace Utility/-e Option
#
##

script()
{
	$dtrace $dt_flags -e -s /dev/stdin <<EOF
	#pragma D option quiet
	BEGIN
	{
		i = 100;
		printf("Value of i: %d\n", i);
		exit(0);
	}
EOF
}

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

script
status=$?

if [ "$status" -ne 0 ]; then
	echo $tst: dtrace failed
fi

exit $status
