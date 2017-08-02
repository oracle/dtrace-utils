#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
script()
{
	$dtrace $dt_flags -q -o $tmpfile -s /dev/stdin <<EOF
	tick-10ms
	{
		printf("%d\n", i++);
	}

	tick-10ms
	/i == 10/
	{
		ftruncate();
	}

	tick-10ms
	/i == 20/
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
tmpfile=/tmp/tst.ftruncate.$$

script
status=$?

cat $tmpfile
rm $tmpfile

exit $status
