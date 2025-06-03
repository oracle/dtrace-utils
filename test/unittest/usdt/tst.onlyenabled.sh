#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@nosort
if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
CFLAGS="$test_cppflags"
LDFLAGS="$test_ldflags"

DIRNAME="$tmpdir/usdt-onlyenabled.$$.$RANDOM"
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
#include <sys/types.h>
#include "prov.h"

int
main(int argc, char **argv)
{
	if (TEST_PROV_GO_ENABLED())
		printf("USDT probe is enabled\n");
	else
		printf("USDT probe is not enabled\n");

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

# Test compiled, but does it run correctly?
echo run without dtrace
./test

echo
echo USDT probes found:
$dtrace $dt_flags -c ./test -lP 'test_prov$target' \
|& awk '/test_prov/ { gsub(/[0-9]+/, "NNN"); print $1, $2, $3, $4, $5; }'

echo
echo run with dtrace but not the USDT probe
$dtrace $dt_flags -c ./test -n 'BEGIN
                                {
                                    printf("BEGIN probe fired\n");
                                    exit(0);
                                }' -o dt.out
cat dt.out     # report dtrace output after trigger output

echo
echo run with dtrace and with the USDT probe
$dtrace $dt_flags -c ./test -n 'test_prov$target:::go
                                {
                                    printf("ERROR: USDT probe fired!\n");
                                    exit(1);
                                }'

exit 0
