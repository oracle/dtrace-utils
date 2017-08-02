#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright © 2007, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
script()
{
	$dtrace $dt_flags -qs /dev/stdin <<EOF
	profile-1234hz
	/arg0 != 0/
	{
		@[mod(arg0)] = count();
	}

	tick-100ms
	/i++ == 20/
	{
		exit(0);
	}
EOF
}

spinny()
{
	while true; do
		/bin/date > /dev/null
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
# The only thing we can be sure of is that some module named "vmlinux"
# did some work -- so that's all we'll check.
#
script | tee /dev/fd/2 | grep vmlinux > /dev/null
status=$? 

kill $child
exit $status
