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
# Testing -ln option with an invalid probe name.
#
# SECTION: dtrace Utility/-ln Option
#
##

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

$dtrace $dt_flags -ln profile
$dtrace $dt_flags -ln vmlinux
$dtrace $dt_flags -ln read
$dtrace $dt_flags -ln begin
$dtrace $dt_flags -ln vmlinux:read
$dtrace $dt_flags -ln syscall:vmlinux:read
$dtrace $dt_flags -ln :vmlinux:
$dtrace $dt_flags -ln fbt:des:des3_crunch_block:return
$dtrace $dt_flags -ln :'read*'::
$dtrace $dt_flags -ln read'{printf("FOUND");}'
exit 0
