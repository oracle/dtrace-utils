#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

tmpfile=$tmpdir/tst.profile_umod.$$

script()
{
	$dtrace $dt_flags -qs /dev/stdin <<EOF
	BEGIN
	{
		printf("dtrace is %d\n", \$pid);
	}
	profile-1234hz
	/arg1 != 0/
	{
		@[umod(arg1), pid] = count();
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

script >& $tmpfile
kill $child

# Check that bash is doing work.
status=0
if ! grep -wq 'bash' $tmpfile; then
	echo ERROR: expected bash functions are missing
	status=1
fi

# Check that modules are unique for each pid that interests us.
dtpid=`awk '/^dtrace is [0-9]*$/ { print $3 }' $tmpfile`
if gawk '$2 == '$child' || $2 == '$dtpid' {print $1, $2}' $tmpfile | sort | uniq -c | grep -qv " 1 "; then
	echo ERROR: duplicate umod
	status=1
fi

if [ $status -ne 0 ]; then
	cat $tmpfile
fi
exit $status
