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
# Without the -Z option probe descriptions that do not match any known
# probes will cause an error or will not be enabled.
#
# SECTION: dtrace Utility/-Z Option
#
##

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

$dtrace $dt_flags -qP wassup'{printf("Iamkool");}' \
-qP profile'{printf("I am done"); exit(0);}'

status=$?

if [ "$status" -ne 0 ]; then
	exit 0
fi

echo $tst: dtrace failed
exit $status
