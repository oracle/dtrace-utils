#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2012, 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@xfail: dtv2

##
#
# ASSERTION:
# The -p option can be used to grab existing processes, and
# catches the process termination event when they exit.
#
# SECTION: dtrace Utility/-p Option
#
##

# @@timeout: 10

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

sleep 5 &

sleepid=$!

$dtrace $dt_flags -q -p $sleepid -n 'proc:::exit /pid == $target/ { printf("exit seen, exitcode %i\n", arg0); }'

status=$?

if [ "$status" -ne 0 ]; then
	echo $tst: dtrace failed
fi

exit $status
