#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2013, 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

# @@timeout: 80

##
#
# ASSERTION:
# Testing -lf option with an invalid function name.
#
# SECTION: dtrace Utility/-lf Option
#
##

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

$dtrace $dt_flags -lf profile
$dtrace $dt_flags -lf vmlinux
$dtrace $dt_flags -lf :vmlinux::
$dtrace $dt_flags -lf ::read:
$dtrace $dt_flags -lf ::'read*':
$dtrace $dt_flags -lf profile:::tick-1000
$dtrace $dt_flags -lf fbt:des:des3_crunch_block:return
# This is curiously different from what man.ListProbesWithFunctions says should
# happen (where no-space-after-read is counted as invalid and space-after-read
# as valid).
$dtrace $dt_flags -lf read '{printf("FOUND");}'
exit 0
