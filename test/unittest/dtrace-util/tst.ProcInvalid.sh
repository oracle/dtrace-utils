#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#


# Assertion:
# The -p option produces a reasonable error message when an invalid PID
# is provided.
#
# SECTION:
#	dtrace Utility/-p Option
#

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

$dtrace $dt_flags -p 484837947 -n 'BEGIN { exit(0); }' || true
