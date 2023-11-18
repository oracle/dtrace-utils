#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

# Determine whether the system respects setting the lockmem limit as root.
# Some systems ignore lowering the limit, which means we cannot make it fail.
# In that case, we report XFAIL.
if $dtrace -xlockmem=1 -n 'BEGIN { exit(0); }' &> /dev/null; then
    exit 67
fi

$dtrace -xlockmem=1 -lvn rawtp:::sched_process_fork |& \
    awk '{ print; }
	 /Cannot retrieve argument count/ { err++; }
	 /lockmem/ { err++; }
	 END { exit(err == 2 ? 1 : 0); }'

exit $?
