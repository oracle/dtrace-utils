#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
#
# ASSERTION:
# The -n option can be used to enable the trace of probes from their names.
#
# SECTION: dtrace Utility/-n Option
#
##

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

$dtrace $dt_flags -n genunix:read

status=$?

echo $status

if [ "$status" -ne 0 ]; then
	exit 0
fi

echo $tst: dtrace failed
exit $status
