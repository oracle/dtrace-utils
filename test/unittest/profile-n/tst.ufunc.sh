#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

script()
{
	$dtrace $dt_flags -qs /dev/stdin <<EOF
	profile-1234hz
	/arg1 != 0/
	{
		@[ufunc(arg1)] = count();
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
# The only thing we can be sure of here is that we caught some function in
# bash doing work.  (This actually goes one step further and assumes that we
# catch some non-static function in bash.)
#
script | tee /dev/fd/2 | grep 'bash`[a-zA-Z_]' > /dev/null
status=$? 

kill $child
exit $status
