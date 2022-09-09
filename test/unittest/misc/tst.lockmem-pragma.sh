#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

ulimit -l 1

for val in 16 1K 262144K 256M unlimited; do
	$dtrace -qs /dev/stdin << EOF
		#pragma D option lockmem=$val

		BEGIN
		{
			@ = avg(1234);
			exit(0);
		}
EOF
	echo $?
done

exit 0
