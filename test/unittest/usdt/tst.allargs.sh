#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2018, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# This test covers all USDT probes fired by the DTRACE_PROBEn macros.
# Arguments values are checked only for first 10 arguments because
# there is support for arg0 ... arg9 only at this moment.

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
CC=/usr/bin/gcc
CFLAGS="-std=gnu89 $test_cppflags"
LDFLAGS="$test_ldflags"

DIRNAME="$tmpdir/usdt-allargs.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

cat > test.c <<EOF
#include <sys/sdt.h>

int
main(int argc, char **argv)
{
	DTRACE_PROBE(test_prov, zero);
	DTRACE_PROBE1(test_prov, one, 1);
	DTRACE_PROBE2(test_prov, two, 2, 3);
	DTRACE_PROBE3(test_prov, three, 4, 5, 7);
	DTRACE_PROBE4(test_prov, four, 7, 8, 9, 10);
	DTRACE_PROBE5(test_prov, five, 11, 12, 13, 14, 15);
	DTRACE_PROBE6(test_prov, six, 16, 17, 18, 19, 20, 21);
	DTRACE_PROBE7(test_prov, seven, 22, 23, 24, 25, 26, 27, 28);
	DTRACE_PROBE8(test_prov, eight, 29, 30, 31, 32, 33, 34, 35, 36);
	DTRACE_PROBE9(test_prov, nine, 37, 38, 39, 40, 41, 42, 43, 44, 45);
	DTRACE_PROBE10(test_prov, ten, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55);
	DTRACE_PROBE11(test_prov, eleven, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66);
	DTRACE_PROBE12(test_prov, twelve, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78);
}
EOF

cat > prov.d <<EOF
provider test_prov {
	probe zero();
	probe one(uintptr_t);
	probe two(uintptr_t, uintptr_t);
	probe three(uintptr_t, uintptr_t, uintptr_t);
	probe four(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
	probe five(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
	probe six(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
	probe seven(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
		    uintptr_t);
	probe eight(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
		    uintptr_t, uintptr_t);
	probe nine(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
		   uintptr_t, uintptr_t, uintptr_t);
	probe ten(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
		  uintptr_t, uintptr_t, uintptr_t, uintptr_t);
	probe eleven(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
		     uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
	probe twelve(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
		     uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
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

$dtrace $dt_flags -c ./test -qs /dev/stdin <<EOF
test_prov\$target:::zero
{
	printf("%s:%s:%s\n", probemod, probefunc, probename);
}

test_prov\$target:::one
{
	printf("%s:%s:%s:%li\n", probemod, probefunc, probename, arg0);
}

test_prov\$target:::two
{
	printf("%s:%s:%s:%li:%li\n", probemod, probefunc, probename, arg0, arg1);
}

test_prov\$target:::three
{
	printf("%s:%s:%s:%li:%li:%li\n", probemod, probefunc, probename, arg0, arg1,
	       arg2);
}

test_prov\$target:::four
{
	printf("%s:%s:%s:%li:%li:%li:%li\n", probemod, probefunc, probename, arg0, arg1,
	       arg2, arg3);
}

test_prov\$target:::five
{
	printf("%s:%s:%s:%li:%li:%li:%li:%li\n", probemod, probefunc, probename,
	       arg0, arg1, arg2, arg3, arg4);
}

test_prov\$target:::six
{
	printf("%s:%s:%s:%li:%li:%li:%li:%li:%li\n", probemod, probefunc, probename,
	       arg0, arg1, arg2, arg3, arg4, arg5);
}

test_prov\$target:::seven
{
	printf("%s:%s:%s:%li:%li:%li:%li:%li:%li:%li\n", probemod, probefunc, probename,
	       arg0, arg1, arg2, arg3, arg4, arg5, arg6);
}

test_prov\$target:::eight
{
	printf("%s:%s:%s:%li:%li:%li:%li:%li:%li:%li:%li\n", probemod, probefunc, probename,
	       arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
}

test_prov\$target:::nine
{
	printf("%s:%s:%s:%li:%li:%li:%li:%li:%li:%li:%li:%li\n", probemod, probefunc, probename,
	       arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
}

test_prov\$target:::ten,
test_prov\$target:::eleven,
test_prov\$target:::twelve
{
	printf("%s:%s:%s:%li:%li:%li:%li:%li:%li:%li:%li:%li:%li\n", probemod, probefunc, probename,
	       arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
}
EOF
status=$?

exit $status
