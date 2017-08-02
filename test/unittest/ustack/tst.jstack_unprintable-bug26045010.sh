#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
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

