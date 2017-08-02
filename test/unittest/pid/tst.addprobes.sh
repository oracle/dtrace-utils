#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# This test verifies that it's possible to add new pid probes to an existing
# pid provider.
#

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

sleep 60 &
pid=$!
disown %+

$dtrace $dt_flags -n pid$pid:libc::entry -n 'tick-1s{exit(0);}'
status=$?

if [ $status -gt 0 ]; then
	exit $status;
fi

$dtrace $dt_flags -n pid$pid:libc::return -n 'tick-1s{exit(0);}'
status=$?

if [ $status -gt 0 ]; then
	exit $status;
fi

kill $pid

exit $status
