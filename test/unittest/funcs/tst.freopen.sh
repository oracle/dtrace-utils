#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
script()
{
	$dtrace $dt_flags -wq -o $tmpfile -s /dev/stdin $tmpfile <<EOF
	BEGIN
	{
		i = 0;
	}

	tick-10ms
	{
		freopen("%s.%d", \$\$1, i);
		printf("%d\n", i)
	}

	tick-10ms
	/++i == $iter/
	{
		freopen("");
		printf("%d\n", i);
		exit(0);
	}
EOF
}

cleanup()
{
	let i=0

	if [ -f $tmpfile ]; then
		rm $tmpfile
	fi

	while [ "$i" -lt "$iter" ]; do
		if [ -f $tmpfile.$i ]; then
			rm $tmpfile.$i
		fi
		let i=i+1
	done
}

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
tmpfile=/tmp/tst.freopen.$$
iter=20

script
status=$?

let i=0

if [ -f $tmpfile.$iter ]; then
	echo "$0: did not expect to find file: $tmpfile.$iter"
	cleanup
	exit 100
fi

mv $tmpfile $tmpfile.$iter
let iter=iter+1

while [ "$i" -lt "$iter" ]; do
	if [ ! -f $tmpfile.$i ]; then
		echo "$0: did not find expected file: $tmpfile.$i"
		cleanup
		exit 101
	fi

	j=`cat $tmpfile.$i`

	if [ "$i" -ne "$j" ]; then
		echo "$0: unexpected contents in $tmpfile.$i: " \
		    "expected $i, found $j"
		cleanup
		exit 102
	fi

	rm $tmpfile.$i
	let i=i+1
done

exit $status
