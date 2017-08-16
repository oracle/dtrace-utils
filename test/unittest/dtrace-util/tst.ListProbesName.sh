#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

# @@timeout: 20

##
#
# ASSERTION:
# Testing -ln option with a valid probe name.
#
# SECTION: dtrace Utility/-ln Option
#
##

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

$dtrace $dt_flags -ln BEGIN
$dtrace $dt_flags -ln :read:
$dtrace $dt_flags -ln syscall::read:
$dtrace $dt_flags -ln ::read:
$dtrace $dt_flags -ln syscall:vmlinux:'read*':
$dtrace $dt_flags -ln profile:::tick-1000
$dtrace $dt_flags -ln read: -n write:
$dtrace $dt_flags -ln read: -n fight:
$dtrace $dt_flags -ln 'syscall::read*:' -n fight:
$dtrace $dt_flags -ln fight: -n write:
$dtrace $dt_flags -ln read:'{printf("FOUND");}'
$dtrace $dt_flags -ln read:entry'{printf("FOUND");}'
$dtrace $dt_flags -ln BEGIN'{printf("FOUND");}'
$dtrace $dt_flags -ln BEGIN'/probename=="entry"/{printf("FOUND");}'
exit 0
