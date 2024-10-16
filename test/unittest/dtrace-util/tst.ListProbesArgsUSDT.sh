#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2013, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

##
#
# ASSERTION:
# Testing -lvn option with USDT probes with a valid probe name.
#
# SECTION: dtrace Utility/-ln Option
#
##

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
CC=/usr/bin/gcc
CFLAGS=

DIRNAME="$tmpdir/list-probes-args-usdt.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

cat > prov.d <<EOF
provider test_prov {
	probe go(int a, char *b) : (char *b, int a);
	probe go_doubled(int a, char *b) : (char *b, int a, int a, char *b);
	probe go_halved(int a, char *b) : (char *b);
	probe go_vanishing(int a, char *b) : ();
};
EOF

$dtrace -h -s prov.d
if [ $? -ne 0 ]; then
	echo "failed to generate header file" >& 2
	exit 1
fi

cat > test.c <<EOF
#include <sys/types.h>
#include "prov.h"

int
main(int argc, char **argv)
{
	TEST_PROV_GO(666, "foo");
	TEST_PROV_GO_DOUBLED(666, "foo");
	TEST_PROV_GO_HALVED(666, "foo");
	TEST_PROV_GO_VANISHING(666, "foo");
	sleep(10);
}
EOF

${CC} ${CFLAGS} -c test.c
if [ $? -ne 0 ]; then
	echo "failed to compile test.c" >& 2
	exit 1
fi
$dtrace -G -s prov.d test.o
if [ $? -ne 0 ]; then
	echo "failed to create DOF" >& 2
	exit 1
fi
${CC} ${CFLAGS} -o test test.o prov.o
if [ $? -ne 0 ]; then
	echo "failed to link final executable" >& 2
	exit 1
fi

script()
{
	$dtrace -c ./test -lvn 'test_prov$target:::go*'
	./test &
	PID=$!
	disown %+
	$dtrace -p $PID -lvn 'test_prov$target:::go*'
	kill -9 $PID
}

script
status=$?

exit $status
