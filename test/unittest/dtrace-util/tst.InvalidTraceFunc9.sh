#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
#
# ASSERTION:
# Testing -f option with an invalid function name.
#
# SECTION: dtrace Utility/-f Option
#
##

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

$dtrace $dt_flags -f read '{printf("FOUND");}'

if [ $? -ne 1 ]; then
	echo $tst: dtrace failed
	exit 1
fi

exit 0
