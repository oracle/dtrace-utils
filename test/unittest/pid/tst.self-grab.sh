#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# This script tests that self-grabs work with the pid provider.
# Note: we expect script compilation to fail!
# We do not expect a coredump.
#
if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

$dtrace $dt_flags -s /dev/stdin >/dev/null <<'EOF'
pid$pid:libc.so:printf:entry
/ execname == "dtrace" /
{
}
tick-5s
{
	exit(0);
}
EOF
exit 0
