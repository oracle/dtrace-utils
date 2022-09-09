#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

for val in 1 262144 unlimited; do
	ulimit -l $val

	$dtrace -qn 'BEGIN { @ = avg(1234); exit(0); }'
	echo $?
done

exit 0
