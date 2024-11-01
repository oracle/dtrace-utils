#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

# The mechanism for discovering rawtp arg types relies first on CTF.
# If CTF information is not available, we fall back to a "trial-and-error"
# method, which really only tells us the number of args.  And that
# method simply cannot work if trials error due to lockmem limits.
# If lockmem limits are encountered, we want to signal that problem to the
# user.
#
# This test checks that failure mode.  Therefore, two conditions
# must be met for this test to work.  First, lockmem limits must be
# respected.  Second, no CTF information should be available, so that
# the trial-and-error method will be invoked.

dtrace=$1

# Determine whether the system respects setting the lockmem limit as root.
# Some systems ignore lowering the limit, which means we cannot make it fail.
# In that case, we report XFAIL.
if $dtrace $dt_flags -xlockmem=1 -n 'BEGIN { exit(0); }' &> /dev/null; then
    exit 67
fi

$dtrace $dt_flags -xlockmem=1 -lvn rawtp:::sched_process_fork |& \
    gawk 'BEGIN {
	     err = 0;  # lockmem error messages
	     CTF = 0;  # arg types indicating CTF info
	     try = 0;  # arg types indicating trial-and-error
	 }

	 { print; }

	 /Cannot retrieve argument count/ { err++; }
	 /lockmem/ { err++; }
	 /^	*args\[[01]\]: struct task_struct \*$/ { CTF++ }
	 /^	*args\[[01]\]: uint64_t$/ { try++ }

	 END {
	     # Skip this test if CTF info was found.
	     if (err == 0 && CTF == 2 && try == 0) exit(67);

	     # Report the expected fail if error message is found.
	     if (err == 2 && CTF == 0 && try == 0) exit(1);

	     # Indicate the expected fail did not occur.
	     exit(0);
	 }'

exit $?
