#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
#
# ASSERTION:
# The -c option errors when used to invoke a nonexistent process.
#
# SECTION: dtrace Utility/-p Option
#
##

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

$dtrace $dt_flags -q -c /nonexistent/process/wibble -P proc
