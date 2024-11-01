#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2008, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# This test verifies that a program that corrupts its own environment
# without inducing a crash does not crash solely due to drti.o's use of
# getenv(3C).
#

PATH=/usr/bin:/usr/sbin:$PATH

if [ $# != 1 ]; then
	echo 'expected one argument: <dtrace-path>'
	exit 2
fi

dtrace="$1"

DIRNAME="$tmpdir/usdt-corruptenv.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

cat > Makefile <<EOF
all: main

main: main.o prov.o
	\$(CC) \$(test_ldflags) -o main main.o prov.o

main.o: main.c prov.h
	\$(CC) \$(test_cppflags) -c main.c

prov.h: prov.d
	$dtrace $dt_flags -h -s prov.d

prov.o: prov.d main.o
	$dtrace $dt_flags -G -s prov.d main.o
EOF

cat > prov.d <<EOF
provider tester {
	probe entry();
};
EOF

cat > main.c <<EOF
#include <stdlib.h>
#include "prov.h"

int
main(int argc, char **argv, char **envp)
{
	envp[0] = (char*)0xff;
	TESTER_ENTRY();
	return 0;
}
EOF

make > /dev/null
status=$?
if [ $status != 0 ]; then
	echo "failed to build" >& 2
else
	./main
	status=$?
fi

exit $status
