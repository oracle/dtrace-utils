#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

for mylocale in C en_US.utf8; do
	export LC_NUMERIC=$mylocale

	$dtrace $dt_flags -qs /dev/stdin << EOF
	BEGIN
	{
		printf("%'d\n", 123456789);
		exit(0);
	}
EOF
	if [ $? -ne 0 ]; then
		echo ERROR: dtrace
		locale
		exit 1
	fi
done

echo success
exit 0
