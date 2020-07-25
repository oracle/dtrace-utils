#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
#
# ASSERTION:
# The -D option can be used to define a name when used in conjunction
# with the -C option. 
#
# SECTION: dtrace Utility/-C Option;
# 	dtrace Utility/-D Option
#
##

script()
{
	$dtrace $dt_flags -C -D VALUE=40 -s /dev/stdin <<EOF
	#pragma D option quiet

	BEGIN
	{
		printf("Value of VALUE: %d\n", VALUE);
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
