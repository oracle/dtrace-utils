#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2018, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
#  @@runtest-opts: -x noresolve 

script()
{
	$dtrace $dt_flags -qs /dev/stdin <<EOF
	profile-1234hz
	/arg1 != 0/
	{
		@[usym(arg1)] = count();
	}

	tick-2s
	{
		exit(0);
	}
EOF
}

spinny()
{
	while true; do
		let i=i+1
	done
}

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

spinny &
child=$!
disown %+

#
# This test is essentially the same as that in the ufunc test; see that
# test for the rationale.
#
script | tee /dev/fd/2 | grep 'bash:[0-9]' > /dev/null
status=$? 

kill $child
exit $status
