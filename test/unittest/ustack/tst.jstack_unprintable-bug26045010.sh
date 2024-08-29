#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2017, 2021, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@xfail: dtv2 jstack not implemented
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
$dtrace -q $dt_flags -n '
BEGIN
{
	printf("hello world\n");
}
profile-9
/execname == "java" && arg1/
{
	jstack(4);
}' -o $file &
pid=$!

# confirm that the DTrace job is running successfully
nsecs=0
while true; do
	sleep 1
	nsecs=$(($nsecs + 1))
	if grep -q "hello world" $file; then
		break
	fi
	if ! ps -p $pid >& /dev/null; then
		echo DTrace died
		cat $file
		rm -f $file
		exit 1
	fi
	if [ $nsecs -ge 10 ] ; then
		echo error starting DTrace job
		kill %1
		cat $file
		rm -f $file
		exit 1
	fi
done

# run the Java job
/usr/bin/java bug26045010

# kill the DTrace job
sleep 1
kill %1
wait

# check results
n=`sed 's/[[:print:]]//g' $file | gawk 'BEGIN {x = 0}; NF>0 {x += 1}; END {print x}'`
if [ $n -gt 0 ]; then
        echo $tst: $n lines have unprintable characters
        sed 's/[[:print:]]//g' $file | gawk 'NF>0'
        echo "==================== file start"
        cat $file
        echo "==================== file end"
        n=1
fi

rm -f $file

exit $n
