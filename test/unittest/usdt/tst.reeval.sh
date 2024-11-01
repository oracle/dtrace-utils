#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@xfail: dtv2, needs sdt
if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
CC=/usr/bin/gcc
CFLAGS="$test_cppflags"
LDFLAGS="$test_ldflags"

DIRNAME="$tmpdir/usdt-reeval.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

cat > test.c <<EOF
#include <sys/sdt.h>

int
main(int argc, char **argv)
{
	DTRACE_PROBE(test_prov, zero);
}
EOF

cat > prov.d <<EOF
provider test_prov {
	probe zero();
};
EOF

${CC} ${CFLAGS} -c test.c
if [ $? -ne 0 ]; then
	echo "failed to compile test.c" >& 2
	exit 1
fi
$dtrace $dt_flags -G -s prov.d test.o
if [ $? -ne 0 ]; then
	echo "failed to create DOF" >& 2
	exit 1
fi
${CC} ${LDFLAGS} -o test test.o prov.o
if [ $? -ne 0 ]; then
	echo "failed to link final executable" >& 2
	exit 1
fi

script()
{
	$dtrace $dt_flags -wZs /dev/stdin <<EOF
	BEGIN
	{
		system("$DIRNAME/test");
	}

	test_prov*:::
	{
		seen = 1;
	}

	proc:::exit
	/progenyof(\$pid) && execname == "test"/
	{
printf("seen = %d", seen);
		exit(seen ? 0 : 2);
	}
EOF
}

script
status=$?
echo $status

exit $status
