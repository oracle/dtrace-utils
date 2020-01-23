#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@xfail: dtv2

##
#
# ASSERTION:
# Using the -q option suppresses the dtrace messages and prints only the
# data traced by trace() and printf() to stdout.
#
# SECTION: dtrace Utility/-q Option
#
##

script()
{
	$dtrace $dt_flags -q -s /dev/stdin <<EOF

	BEGIN
	{
		printf("I am the only one.");
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
