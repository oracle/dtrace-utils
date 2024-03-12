#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

############################################################################
# ASSERTION:
# 	ufunc() values sort as expected
#
# SECTION: Aggregations, Printing Aggregations
#
# NOTES: Assumes that the order of user function addresses will follow
#	the function declaration order.
#
############################################################################

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

$dtrace $dt_flags -c test/triggers/profile-tst-ufuncsort -s /dev/stdin <<EOF


	#pragma D option quiet
	#pragma D option aggsortkey

	pid\$target::fN:entry
	{
		@[ufunc(arg0)] = count();
	}
	tick-1s
	{
		exit(0);
	}
EOF

status=$?
if [ "$status" -ne 0 ]; then
	echo $tst: dtrace failed
	exit $status
fi

exit $status
