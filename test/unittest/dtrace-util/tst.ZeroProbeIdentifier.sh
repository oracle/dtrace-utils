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
# Testing -i option with zero probe identifier.
#
# SECTION: dtrace Utility/-i Option
#
##

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

$dtrace $dt_flags -i 0

if [ $? -ne 1 ]; then
	echo $tst: dtrace failed
	exit 1
fi

exit 0
