#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
CC=/usr/bin/gcc
CFLAGS="$test_cppflags"
LDFLAGS="$test_ldflags"
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
$dtrace $dt_flags -G -s manyprobes.d test.o
if [ $? -ne 0 ]; then
	echo "failed to create DOF" >& 2
	exit 1
fi
${CC} ${LDFLAGS} -o test test.o manyprobes.o
if [ $? -ne 0 ]; then
	echo "failed to link final executable" >& 2
	exit 1
fi

script()
{
	$dtrace $dt_flags -c ./test -qs /dev/stdin <<EOF
	manyprobes\$target:::test1, manyprobes\$target:::test750, manyprobes\$target:::test1999
	{
		printf("%s:%s:%s\n", probemod, probefunc, probename);
	}
EOF
}

script
status=$?

exit $status
