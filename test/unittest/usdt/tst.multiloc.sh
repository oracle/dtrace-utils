#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# This test covers USDT probes firing from multiple locations in the same
# function, verifying that argument data is correct.

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
CFLAGS="-std=gnu89 $test_cppflags"
LDFLAGS="$test_ldflags"

DIRNAME="$tmpdir/usdt-multiloc.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

cat > test.c <<EOF
#include <sys/sdt.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
	printf("%s\n", __func__);
	DTRACE_PROBE1(test_prov, args, __func__);
	DTRACE_PROBE1(test_prov, args, __func__);
}
EOF

cat > prov.d <<EOF
provider test_prov {
	probe args(char *);
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

$dtrace $dt_flags -c './test arg1val' -qs /dev/stdin <<EOF
test_prov\$target:::args
{
	printf("%s:%s:%s %s\n", probemod, probefunc, probename,
	       copyinstr(arg0));
}

EOF
status=$?

exit $status
