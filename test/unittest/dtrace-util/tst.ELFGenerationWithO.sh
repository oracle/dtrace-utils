#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@xfail: dtv2

##
#
# ASSERTION:
# Using -G option with dtrace utility produces an ELF file containing a
# DTrace program. The output file can be named as required using the
# -o option in conjunction with the -G option.
#
# SECTION: dtrace Utility/-G Option;
# 	dtrace Utility/-o Option
#
##

script()
{
	$dtrace $dt_flags -G -o outputFile -s /dev/stdin <<EOF
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

if [ ! -e "outputFile" ]; then
	echo $tst: file not generated
	exit 1
fi

rm -f "outputFile"
exit 0
