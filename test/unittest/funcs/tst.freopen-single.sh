#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# ASSERTION: freopen should work even if it is the only action in the clause.
#

dtrace=$1

$dtrace $dt_flags -wqn '
tick-600ms
{
	freopen("/dev/null");
}
tick-700ms
{
	printf("hello world\n");
}
tick-800ms
{
	freopen("");
}
tick-900ms
{
	printf("the quick brown fox jumped over the lazy dog\n");
	exit(0);
}'

if [ $? -ne 0 ]; then
	echo DTrace failed
	exit 1
fi

