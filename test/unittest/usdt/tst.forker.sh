#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
#
# @@timeout: 120

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

#
# Test that we don't deadlock between forking and enabling/disabling USDT
# probes. This test has succeeded if it completes at all.
#

./test/triggers/usdt-tst-forker &
id=$!
disown %+

while kill -0 $id >/dev/null 2>&1; do
	$dtrace $dt_flags -p $id -s /dev/stdin <<-EOF
		forker*:::fire
		/i++ == 4/
		{
			exit(0);
		}
	EOF
	if [ $? -ne 0 ]; then
		echo ERROR: DTrace failed to run
		exit 1
	fi
done

exit 0
