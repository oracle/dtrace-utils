#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

tmpfile=$tmpdir/tst.profile_ufunc.$$

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

script >& $tmpfile
kill $child

# Check that some non-static function in bash is doing work.
status=0
if ! grep -q 'bash`[a-zA-Z_]' $tmpfile; then
	echo ERROR: expected bash functions are missing
	status=1
fi

# Check that functions are unique.  (Exclude shared libraries and unresolved addresses.)
if awk '!/^ *lib/ && !/^ *0x/ {print $1}' $tmpfile | sort | uniq -c | grep -qv " 1 "; then
	echo ERROR: duplicate ufunc
	status=1
fi

if [ $status -ne 0 ]; then
	cat $tmpfile
fi

exit $status
