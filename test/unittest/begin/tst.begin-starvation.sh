#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@ xfail: BEGIN not yet forced to be processed first
#

#
# This script tests that BEGIN in conjunction with a high-volume probe
# processes (at least one) BEGIN first.
#
if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
# Use DTRACE_DEBUG to make dtrace itself do a lot of write output and
# slow it down a lot.
DTRACE_DEBUG=t exec $dtrace $dt_flags -s /dev/stdin >/dev/null 2>&1 <<EOF
#pragma D option bufsize=32k
syscall::write:entry
{
	exit(1);
}
BEGIN
{
	exit(0);
}
EOF
