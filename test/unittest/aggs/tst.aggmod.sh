#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# This test case verifies that the mod() aggregating function normalizes address
# correctly.  The goal is to find two symbols that belong to different ranges in
# the libdtrace's addr->module mapping (for both the text section and the data
# section).  The simple D script then aggregates all four values
#

if [[ $# != 1 ]]; then
	echo "expected one argument: <dtrace-path>"
	exit 2;
fi

file=/tmp/out.$$
dtrace=$1

#
# Scan the /proc/kallmodsyms using the following rules:
#
#   1. Consider only kernel symbol entries (The fifth field is empty).
#   2. Take only symbols with size > 0 into account.
#   3. The symbol type must match the find_addresses() argument.
#
# The algorithm iterates over every kernel symbol line and keeps the symbol
# address in the lastaddr variable.  Either it iterates over symbol from modules
# (the fifth field contains [xxx]) it stops at first kernel symbol we encounter.
#
# Once the iterator stops the current line points to symbol addr from new range
# and lastaddr contains address of symbol from previous range.
#
function find_addresses() {
    echo `awk '
$2 != "0" && $3 ~ /['\$1']/ && NF == 4 {
	if (lastmod != "") {
		print "0x" lastaddr " 0x" $1;
		exit 0;
	}
	lastaddr = $1;
}

{ lastmod = $5;}
' /proc/kallmodsyms`
}

taddr=$(find_addresses "tT")
daddr=$(find_addresses "dD")

#
# Now we run DTrace and aggregate on both addresses by using mod().
#

rm -f $file

$dtrace $dt_flags -o $file -s /dev/stdin $taddr $daddr <<\EOF

#pragma option D quiet

BEGIN
{
	@[mod($1)] = count();
	@[mod($2)] = count();
	@[mod($3)] = count();
	@[mod($4)] = count();
	exit(0);
}
EOF

#
# Verify results
#
# There should be only 1 vmlinux key printed out.
#
status=$?
if [[ "$status" -ne 0 ]]; then
	echo $tst: dtrace failed
	rm -f $file
	exit $status
fi

count=`grep -c 'vmlinux' $file`

if [[ "$count" != 1 ]]; then
	echo "aggregating by mod() contains invalid vmlinux key count."
	exit 1;
fi

rm -f $file
exit $status
