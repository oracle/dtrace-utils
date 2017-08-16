#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

# @@xfail: __gnuc_va_list problem with stdio inclusion

##
#
# ASSERTION:
# The -C option is used to run the C preprocessor over D programs before
# compiling them. The -H option used in conjuction with the -C option
# lists the pathnames of the included files to STDERR.
#
# SECTION: dtrace Utility/-C Option;
# 	dtrace Utility/-H Option
#
##

script()
{
	$dtrace $dt_flags -CH -s /dev/stdin <<EOF

#include <stdio.h>

	BEGIN
	{
		printf("This test should compile\n");
		exit(0);
	}
EOF
}

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

script
status=$?

if [ "$status" -ne 0 ]; then
	echo $tst: dtrace failed
fi

exit $status
