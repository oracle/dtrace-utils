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
# Using -G option with dtrace utility produces an ELF file containing a
# DTrace program. If the filename used with the -s option does not end
# with .d, and the -o option is not used, then the output ELF file is
# in d.out.
#
# SECTION: dtrace Utility/-G Option
#
##

script()
{
	$dtrace $dt_flags -G -s /dev/stdin <<EOF
	BEGIN
	{
		printf("This test should compile.\n");
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
	exit $status
fi

if [ ! -e "d.out" ]; then
	echo $tst: file not generated
	exit 1
fi

rm -f "d.out"
exit 0
