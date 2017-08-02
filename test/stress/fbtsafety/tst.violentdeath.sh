#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright © 2007, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@skip: blitzed by test.options

script()
{
	$dtrace $dt_flags -x bufpolicy=ring -x bufsize=1k -s /dev/stdin <<EOF
	fbt:::
	{}
EOF
}

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

let i=0

while [ "$i" -lt 10 ]; do
	script &
	child=$!
	disown %+
	sleep 1
	kill -9 $child
	let i=i+1
done
