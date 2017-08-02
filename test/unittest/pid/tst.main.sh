#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

#
# Make sure we can trace main:entry properly -- this was problematic because
# we also set a breakpoint on the same spot in libdtrace.
#

$dtrace $dt_flags -c date -s /dev/stdin <<EOF
	BEGIN
	{
		status = 1;
	}

	pid\$target::main:entry
	{
		status = 0;
	}

	END
	{
		exit(status);
	}
EOF

exit $?
