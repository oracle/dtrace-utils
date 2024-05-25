#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# Rebuilding an object file containing DOF changes slightly when the object
# files containing the probes have already been modified. This tests that
# case by generating the DOF object, removing it, and building it again.

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
CC=/usr/bin/gcc
CFLAGS="$test_cppflags"

DIRNAME="$tmpdir/usdt-static2.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

cat > test.c <<EOF
#include <unistd.h>
#include <sys/sdt.h>

static void
foo(void)
{
	DTRACE_PROBE(test_prov, probe1);
	DTRACE_PROBE(test_prov, probe2);
}

int
main(int argc, char **argv)
{
	DTRACE_PROBE(test_prov, probe1);
	DTRACE_PROBE(test_prov, probe2);
	foo();
}
EOF

cat > prov.d <<EOF
provider test_prov {
	probe probe1();
	probe probe2();
};
EOF

${CC} ${CFLAGS} -c test.c
if [ $? -ne 0 ]; then
	echo "failed to compile test.c" >& 2
	exit 1
fi
$dtrace -G -s prov.d test.o
if [ $? -ne 0 ]; then
	echo "failed to create initial DOF" >& 2
	exit 1
fi
rm -f prov.o
$dtrace -G -s prov.d test.o
if [ $? -ne 0 ]; then
	echo "failed to create final DOF" >& 2
	exit 1
fi
${CC} ${CFLAGS} -o test test.o prov.o
if [ $? -ne 0 ]; then
	echo "failed to link final executable" >& 2
	exit 1
fi

script()
{
	$dtrace -c ./test -qs /dev/stdin <<EOF
	test_prov\$target:::
	{
		printf("%s:%s:%s\n", probemod, probefunc, probename);
	}
EOF
}

script
status=$?

exit $status
