#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2017, 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@xfail: dtv2

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

file=$tmpdir/out.$$
dtrace=$1

rm -f $file

#
# 26268136 dtrace_aggregate_lquantize() suffers from 32-bit overflow
#
# The probe increments values for buckets <2, 2, 4, 6, 8, and >=10.
# The only data value it encounters is 0x1080000000ull, which should
# appear in the topmost bucket, producing the distribution:
#
#          value  ------------- Distribution ------------- count
#              8 |                                         0
#          >= 10 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ 1
#
# The bug is that we see only the bottom 32 bits, the highest of which
# is 1 and therefore indicating a negative number.  This bug produces:
#
#          value  ------------- Distribution ------------- count
#            < 2 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ 1
#              2 |                                         0
#

$dtrace $dt_flags -o $file -c "sleep 1" -s /dev/stdin <<EOF
	#pragma D option quiet

	proc:::exit
	{
		@ = lquantize(0x1080000000ull, 2, 10, 2);
	}
EOF

status=$?
if [ "$status" -ne 0 ]; then
	echo $tst: dtrace failed
	rm -f $file
	exit $status
fi

if [ `grep -c " < 2 " $file` -gt 0 ]; then
	echo "$tst: large value counted in <2 bucket"
	cat $file
	status=1
fi

rm -f $file

exit $status

