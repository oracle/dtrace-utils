#!/bin/bash

#
# Oracle Linux DTrace.
# Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# ASSERTION:
# If a macro argument results in an integer overflow, a D_MACRO_OFLOW is
# thrown.
#
# SECTION: Errtags/D_MACRO_OFLOW
#

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

$dtrace $dt_flags -n 'BEGIN { $1; }' 0x12345678123456781
status=$?

if [ "$status" -ne  0 ]; then
	exit 0
fi

echo dtrace failed
exit $status
