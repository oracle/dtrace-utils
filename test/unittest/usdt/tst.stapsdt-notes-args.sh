#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# This test covers stapsdt probes fired by the STAP_PROBEn macros,
# testing various argument forms (constant, register, deref etc).

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
CC=/usr/bin/gcc
CFLAGS="-I${PWD}/test/unittest/usdt"

DIRNAME="$tmpdir/usdt-notes.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

cat > test.c <<EOF
#include <sdt_notes.h>

int
main(int argc, char **argv)
{
	STAP_PROBE4(test_prov, args, argc, argv[0], argv[1] + 4, 18);
}
EOF

${CC} ${CFLAGS} -o test test.c
if [ $? -ne 0 ]; then
	echo "failed to compile test.c" >& 2
	exit 1
fi

$dtrace -c './test arg1val' -qs /dev/stdin <<EOF
test_prov\$target:::args
{
	printf("%s:%s:%s:%li:%s:%s:%li\n", probemod, probefunc, probename,
	       arg0, copyinstr(arg1), copyinstr(arg2), arg3);
}

EOF
status=$?

exit $status
