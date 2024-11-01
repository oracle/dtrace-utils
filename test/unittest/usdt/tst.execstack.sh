#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
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
OBJDUMP=/usr/bin/objdump
READELF=/usr/bin/readelf

DIRNAME="$tmpdir/usdt-execstack.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

cat > prov.d <<EOF
provider test_prov {
	probe zero();
	probe one(uintptr_t);
	probe u_nder(char *);
	probe d__ash(char **);
};
EOF

$dtrace $dt_flags -h -s prov.d
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
	TEST_PROV_ZERO();
	TEST_PROV_ONE(1);
	TEST_PROV_U_NDER("hi there");
	TEST_PROV_D_ASH(argv);
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
${OBJDUMP} -hj .note.GNU-stack prov.o | grep EXEC
if [ $? -eq 0 ]; then
	echo "EXEC flag on .note.GNU-stack" >& 2
	exit 1
fi
${CC} ${lDFLAGS} -o test test.o prov.o
if [ $? -ne 0 ]; then
	echo "failed to link final executable" >& 2
	exit 1
fi
${READELF} -Wl test | grep STACK | grep RWE
if [ $? -eq 0 ]; then
	echo "EXEC flag on STACK" >& 2
	exit 1
fi

exit 0
