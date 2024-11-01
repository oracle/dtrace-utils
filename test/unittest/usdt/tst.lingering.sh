#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# Test multiple probes, repeatedly invoked by multiple dtraces
# on the same running trigger
#
if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
CC=/usr/bin/gcc
CFLAGS="$test_cppflags"
LDFLAGS="$test_ldflags"

DIRNAME="$tmpdir/usdt-lingering.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

cat > prov.d <<EOF
provider test_prov {
	probe go();
	probe go2();
};
EOF

$dtrace $dt_flags -h -s prov.d
if [ $? -ne 0 ]; then
	echo "failed to generate header file" >& 2
	exit 1
fi

cat > test.c <<EOF
#include <sys/types.h>
#include <unistd.h>
#include "prov.h"

int
main(int argc, char **argv)
{
	for (;;) {
		TEST_PROV_GO();
		TEST_PROV_GO2();
		sleep(1);
	}

	return 0;
}
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

./test &
TRIGPID=$!

trap 'kill -9 $TRIGPID' ERR EXIT

script() {
	$dtrace $dt_flags -p $TRIGPID -qs /dev/stdin <<EOF
	test_prov\$target:::$1
	{
		printf("%s:%s:%s\n", probemod, probefunc, probename);
		exit(0);
	}

	tick-5s
	{
		printf("probe $1 timed out\n");
		exit(1);
	}
EOF
}

set -e
script go
script go2
script go2
script go
