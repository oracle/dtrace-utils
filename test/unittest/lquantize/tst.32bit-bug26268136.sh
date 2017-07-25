#!/bin/bash
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright 2017 Oracle, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"%Z%%M%	%I%	%E% SMI"

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

file=/tmp/out.$$
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

