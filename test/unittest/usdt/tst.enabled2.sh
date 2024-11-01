#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2008, 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This test is primarily intended to verify a fix for SPARC, but there's no
# harm in running it on other platforms. Here, we verify that is-enabled
# probes don't interfere with return values from previously invoked functions.

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
CC=/usr/bin/gcc
CFLAGS="$test_cppflags"
LDFLAGS="$test_ldflags"

DIRNAME="$tmpdir/usdt-enabled2.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

cat > prov.d <<EOF
provider test_prov {
	probe go();
};
EOF

$dtrace $dt_flags -h -s prov.d
if [ $? -ne 0 ]; then
	echo "failed to generate header file" >& 2
	exit 1
fi

cat > test.c <<EOF
#include <stdio.h>
#include "prov.h"

int
foo(void)
{
	return 24;
}

int
main(int argc, char **argv)
{
	int a = foo();
	if (TEST_PROV_GO_ENABLED()) {
		TEST_PROV_GO();
	}
	printf("%d %d %d\n", a, a, a);

	return 0;
}
EOF

${CC} ${CFLAGS} -c -O2 test.c
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
	./test

	$dtrace $dt_flags -c ./test -qs /dev/stdin <<EOF
	test_prov\$target:::
	{
	}
EOF
}

script
status=$?

exit $status
