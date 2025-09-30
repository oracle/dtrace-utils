#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

#
# ASSERTION: The -xjstackframes option value must be within the system limit.
#
# SECTION: Options and Tunables/Consumer Options
#
maxframes=$[ `cat /proc/sys/kernel/perf_event_max_stack` + 1 ]

$dtrace $dt_flags -xjstackframes=$maxframes -qn '
BEGIN, ERROR
{
	trace("ERROR: DTrace should fail to start.");
	exit(0);
}'

exit $?
