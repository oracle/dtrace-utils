#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
CC=/usr/bin/gcc
CFLAGS="-I${PWD}/uts/common"
DIRNAME="$tmpdir/usdt-manyprobes.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

gen_test()
{
	local n=$1
	echo "#include <sys/sdt.h>"
	echo "void main(void) {"
	for i in $(seq 1 $n); do
		echo 'DTRACE_PROBE1(manyprobes, 'test$i, '"foo");'
	done
	echo "}"
}

gen_provider()
{
	local n=$1
	echo "provider manyprobes {"
	for i in $(seq 1 $n); do
		echo "probe test$i(string);"
	done
	echo "};"
}

nprobes=2000
gen_test $nprobes > test.c
gen_provider $nprobes > manyprobes.d

${CC} ${CFLAGS} -c test.c
if [ $? -ne 0 ]; then
	echo "failed to compile test.c" >& 2
	exit 1
fi
$dtrace -G -s manyprobes.d test.o
if [ $? -ne 0 ]; then
	echo "failed to create DOF" >& 2
	exit 1
fi
${CC} ${CFLAGS} -o test test.o manyprobes.o
if [ $? -ne 0 ]; then
	echo "failed to link final executable" >& 2
	exit 1
fi

script()
{
	$dtrace -o D.output -c ./test -qs /dev/stdin <<EOF
	BEGIN
	{
		/* Dump pid for the clean-up hack we use. */
		printf("pid is %d\n", \$target);
	}
	manyprobes\$target:::test1, manyprobes\$target:::test750, manyprobes\$target:::test1999
	{
		printf("%s:%s:%s\n", probemod, probefunc, probename);
	}
EOF
}

script
status=$?

# D.output has the pid we need for the clean-up hack.  Display the output minus
# that pid information for checking with the .r results file.  Once uprobe cleanup
# has been automated, the pid info and D.output will not be needed.
grep -v "pid is " D.output

# Here is the clean-up hack for uprobe_events until dtprobed does clean up.
# Find the events for the specified pid and eliminate them.
pid=`awk '/pid is / {print $3}' D.output`
uprobes=/sys/kernel/debug/tracing/uprobe_events
for x in `awk '/^p:dt_pid\/.* \/proc\/'$pid'\/map_files\// { sub("^p:", "-:"); print $1 }' $uprobes`; do
	echo $x >> $uprobes
done

exit $status
