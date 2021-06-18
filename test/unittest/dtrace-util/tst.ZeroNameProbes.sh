#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
#
# ASSERTION:
# The -Z option can be used to permit descriptions that match
# zero probes.
#
# SECTION: dtrace Utility/-Z Option;
# 	dtrace Utility/-n Option
#
##

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

$dtrace $dt_flags -qZn wassup'{printf("Iamkool");}' \
-qn BEGIN'{printf("I am done"); exit(0);}'
status=$?

if [ "$status" -ne 0 ]; then
	echo $tst: dtrace failed
fi

exit $status
