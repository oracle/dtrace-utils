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
tmpfile=${tmpdir:-/tmp}/tst.alloca0-overtainting.$$

$dtrace $dt_flags -xtree=4 -s /dev/stdin > $tmpfile 2>&1 <<EOT
BEGIN
{
	bar = alloca(10);
	trace(bar);
	exit(0);
}
EOT

grep -q 'FUNC trace.*ALLOCA' $tmpfile
status=$?

rm $tmpfile

if [[ $status -gt 0 ]]; then
    exit 0
else
    exit 1
fi
