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
		@[func(arg0)] = count();
	}

	tick-100ms
	/i++ == 50/
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
# This is gutsy -- we're assuming that find_vma will show up in the
# output.
#
script | tee /dev/fd/2 | grep find_vma > /dev/null
status=$?

kill $child
exit $status
