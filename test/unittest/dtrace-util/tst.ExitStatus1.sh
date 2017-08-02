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
# When a fatal error occurs such that the program compilation fails or the
# specified request cannot be satisfied, an exit status of 1 is returned.
#
#
# SECTION: dtrace Utility/Exit Status
#
##

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

$dtrace $dt_flags -s wassup  
status=$?

if [ "$status" -ne 1 ]; then
	echo $tst: dtrace failed
	exit 1
fi

exit 0
