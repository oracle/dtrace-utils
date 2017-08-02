#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
#
# ASSERTION:
# If the -32 option is specified, dtrace will force the D compiler to
# compile a program using the 32-bit data model.
#
# SECTION: dtrace Utility/-32 Option
#
##

script()
{
	$dtrace $dt_flags -32 -s /dev/stdin <<EOF
	BEGIN
	/4 != sizeof(long)/
	{
		printf("Not targeted for 32 bit machine\n");
		exit(1);
	}

	BEGIN
	/4 == sizeof(long)/
	{
		printf("Targeted for 32 bit machine\n");
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
