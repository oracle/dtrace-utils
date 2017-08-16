#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
script()
{
	$dtrace $dt_flags -wq -o $tmpfile -s /dev/stdin 2> $errfile <<EOF
	BEGIN
	{
		/*
		 * All of these should fail...
		 */
		freopen("..");
		freopen("%s", ".");
		freopen("%c%c", '.', '.');
		freopen("%c", '.');

		/*
		 * ...so stdout should still be open here.
		 */
		printf("%d", ++i);

		freopen("%s%s", ".", ".");
		freopen("%s%s", ".", ".");

		printf("%d", ++i);
	}

	BEGIN
	/i == 2/
	{
		/*
		 * ...and here.
		 */
		printf("%d\n", ++i);
		exit(0);
	}

	BEGIN
	{
		exit(1);
	}
EOF
}

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
tmpfile=/tmp/tst.badfreopen.$$
errfile=/tmp/tst.badfreopen.$$.stderr

script
status=$?

if [ "$status" -eq 0 ]; then
	i=`cat $tmpfile`

	if [[ $i != "123" ]]; then
		echo "$0: unexpected contents in $tmpfile: " \
		    "expected 123, found $i"
		status=100
	fi
	
	i=`wc -l $errfile | awk '{ print $1 }'`

	if [ "$i" -lt 6 ]; then
		echo "$0: expected at least 6 lines of stderr, found $i lines"
		status=101
	fi
else
	cat $errfile > /dev/fd/2
fi

rm $tmpfile $errfile

exit $status
