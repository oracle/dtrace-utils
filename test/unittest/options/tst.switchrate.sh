#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

# @@nosort

dtrace=$1

# Currently, there are few easily visible effects of changing the switchrate.
# One way to check is to run a very simple D script that should run
# "instantaneously" -- except that its termination will be delayed by a
# very long switchrate.

status=0
for nexpect in 1 16; do
	# Run the "instantaneous" D script with the prescribed switchrate.
	# Time it.  Round to the nearest number of seconds with int(t+0.5).
	nactual=`/usr/bin/time -f "%e" \
	    $dtrace $dt_flags -xswitchrate=${nexpect}sec -qn 'BEGIN { exit(0) }' \
	    |& gawk 'NF != 0 {print int($1 + 0.5)}'`

	# Check the actual number of seconds to the expected value.
	# Actually, the actual time might be a few seconds longer than expected.
	# So pad $nexpect.
	test/utils/check_result.sh $nactual $(($nexpect + 3)) 4
	status=$(($status + $?))
done

exit $status
