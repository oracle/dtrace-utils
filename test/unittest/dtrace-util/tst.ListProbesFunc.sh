#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2013, 2021, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
#
# ASSERTION:
# Testing -lf option with a valid function name.
#
# SECTION: dtrace Utility/-lf Option
#
##

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

$dtrace $dt_flags -lf read
$dtrace $dt_flags -lf :read
$dtrace $dt_flags -lf ::read
$dtrace $dt_flags -lf syscall::read
$dtrace $dt_flags -lf read -f write
$dtrace $dt_flags -lf read -f fight
$dtrace $dt_flags -lf 'syscall::read*' -f fight
$dtrace $dt_flags -lf fight -f write
$dtrace $dt_flags -lf read'{printf("FOUND");}'
$dtrace $dt_flags -lf read'/probename == "entry"/{printf("FOUND");}'
exit 0
