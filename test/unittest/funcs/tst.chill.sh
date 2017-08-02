#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace_script()
{
	
	$dtrace $dt_flags -w -s /dev/stdin <<EOF

	/*
 	* ASSERTION:
 	*	Positive test of chill()
 	*
 	* SECTION: Actions and Subroutines/chill()
 	* 
 	* NOTES: This test does no verification - it's not possible.  So,
 	* 	we just run this and make sure it runs.
 	*/

	BEGIN 
	{
		i = 0;
	}

	syscall:::entry
	/i <= 5/
	{
		chill(100000000);
		i++;
	}

	syscall:::entry
	/i > 5/
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

dtrace_script &
child=$!

wait $child
status=$?

exit $status
