#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# 20 invocations of a script taking at least two seconds each,
# plus quite a lot of slop for initialization overhead and system load
#
# @@timeout: 90

#
# This script attempts to tease out a race when probes are initially enabled.
#
script()
{
	#
	# Nauseatingly, the #defines below must be in the 0th column to
	# satisfy the ancient cpp that -C defaults to.
	#
	$dtrace $dt_flags -C -s test/unittest/misc/tst.enablerace.dpp
}

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
let i=0

while [ "$i" -lt 20 ]; do
	script
	status=$?

	if [ "$status" -ne 0 ]; then
		exit $status
	fi

	let i=i+1
done

exit 0
