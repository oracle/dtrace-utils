#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@nosort

dtrace=$1

for val in 1 268435456 unlimited; do
	ulimit -l $val

	$dtrace $dt_flags -qn 'BEGIN { @ = avg(1234); exit(0); }'
	echo $?
done

exit 0
