#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# This test covers stapsdt probes fired by the STAP_PROBEn macros, verifying
# that argument data is processed correctly when the same probe is used in
# multiple locations.

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
CFLAGS="-std=gnu89 -I${PWD}/test/unittest/usdt $test_cppflags"
LDFLAGS="$test_ldflags"

DIRNAME="$tmpdir/usdt-notes-bug38922360.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

cat > test.c <<EOF
#include <sdt_notes.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
	printf("%s\n", __func__);
	STAP_PROBE1(test_prov, args, __func__);
	STAP_PROBE1(test_prov, args, __func__);
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
	printf("%s:%s:%s %s\n", probemod, probefunc, probename,
	       copyinstr(arg0));
}

EOF
status=$?

exit $status
