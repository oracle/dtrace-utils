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
dtrace=$1

#
# 26045010 DTrace jstack() may show unprintable characters
#

DIRNAME="$tmpdir/jstack-bug26045010.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

# trigger is just some Java computationally dense loop
cat > bug26045010.java  <<EOF
class bug26045010
{
    public static void main(String args[])
    {
        double x = 0;
        for (int i = 0; i < 1000000000; i++) {
            x = 0.2 * x + 0.3;
        }
        if (x > 37) {
            System.out.println("ERROR!  x = " + x);
        }
    }
}
EOF

/usr/bin/javac bug26045010.java

# cannot use -c with multithreaded code due to
#     20045149 DTRACE: MULTITHREADED PROCESSES DOING DLOPEN() OR DLCLOSE() CRASH
# so put dtrace in the background and kill it when the load finishes

file=out.txt
rm -f $file
$dtrace -q $dt_flags -n 'profile-9 /execname == "java" && arg1/ { jstack(4); }' -o $file &
sleep 1
/usr/bin/java bug26045010
sleep 1
kill %1
sleep 1

status=$?
if [ "$status" -ne 0 ]; then
        echo $tst: dtrace failed
        rm -f $file
        exit $status
fi

n=`sed 's/[ 0-9a-zA-Z_\`\+\.]//g' $file | awk '{x += NF} END {print x}'`
if [ "$n" -gt 0 ]; then
        echo $tst: $n lines have unprintable characters
        echo "==================== file start"
        cat $file
        echo "==================== file end"
        status=1
fi

rm -f $file

exit $status

