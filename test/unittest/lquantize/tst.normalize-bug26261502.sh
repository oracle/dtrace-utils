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
# 26261502 DTrace lquantize() normalization is wrong for name of lowest bucket
#
# The profile probe should alternate between x=6 and x=9.  The lower bound of lquantize
# is "9", however, and so the buckets that should be incremented are "< 9" and "9".
# E.g.
#
#          value  ------------- Distribution ------------- count
#            < 9 |@@@@@@@@@@@@@@@@@@@@                     (some value)
#              9 |@@@@@@@@@@@@@@@@@@@@                     (some value)
#          >= 12 |                                         0
#
# The bug is that adding a "normalize()" function normalizes not only the
# values in the buckets but also the name of the lowest bucket "< 9".
# E.g. "normalize(@,3)" incorrectly renames it "< 3".
#

$dtrace $dt_flags -o $file -c "sleep 10" -s /dev/stdin <<EOF
	#pragma D option quiet

	BEGIN
	{
		x = 9;
	}
	profile:::profile-9
	{
		@ =  lquantize(x, 9, 12, 3);
		x = 15 - x;
	}
	END
	{
		normalize(@, 3);
	}
EOF

status=$?
if [ "$status" -ne 0 ]; then
	echo $tst: dtrace failed
	rm -f $file
	exit $status
fi

n=`awk '/ < / {print $2}' $file`
if [ "$n" -ne 9 ]; then
	echo $tst: lowest-bucket name should be '"< 9"' but is '"< '$n'"'
	cat $file
	status=1
fi

rm -f $file

exit $status

