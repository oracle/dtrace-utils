#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2016, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# This script tests that self-grabs work and do not disturb the death-counter
# that causes dtrace -c to exit when its children die (nor fail in any other
# way), even when DTrace is artificially forced to process huge amounts of
# I/O coming from itself (thus grabbing itself a great many times).
#
if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
exec $dtrace $dt_flags -c 'sleep 2' -s /dev/stdin >/dev/null <<EOF
#pragma D option bufsize=32k
syscall::write:entry
{
	ustack(1);
}
tick-20s
{
	exit(1);
}
EOF
