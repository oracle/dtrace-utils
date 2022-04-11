#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
tmpfile=${tmpdir:-/tmp}/tst.alloca0-values.$$

$dtrace $dt_flags -o $tmpfile -s /dev/stdin <<EOT
BEGIN {
	trace(alloca(0));
	trace(s = alloca(0));
	trace(s);
	trace(alloca(0));

	exit(0);
}
EOT

awk '/:BEGIN/ && $2 == $3 && $3 == $4 && $4 == $5 { exit(0); }
     /:BEGIN/ { print; exit(1); }' $tmpfile

status=$?
rm $tmpfile

exit $status
