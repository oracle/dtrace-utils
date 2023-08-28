#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

$dtrace $dt_flags -Sen '
sdt:task::task_rename
{
	htons(1234);
	htonl(1234);
	htonll(1234);
}
' 2>&1 | \
	awk '/ tobe / {
                sub(/^[^:]+: /, "");
		sub(/^dc [0-9] /, "dc X ");
		sub(/%r[0-9],/, "%rX,");
		print;
	     }'

exit $?
