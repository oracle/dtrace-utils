#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# This test covers all stapsdt probes fired by the STAP_PROBEn macros.
# Arguments values are checked only for first 10 arguments because
# there is support for arg0 ... arg9 only at this moment.

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
#include <stdio.h>
#define _SDT_HAS_SEMAPHORES 1
#include <sdt_notes.h>

unsigned short test__prov_zero__probe_semaphore = 0;
unsigned short test__prov_one__probe_semaphore = 0;
unsigned short test__prov_two__probe_semaphore = 0;
unsigned short test__prov_three__probe_semaphore = 0;
unsigned short test__prov_four__probe_semaphore = 0;
unsigned short test__prov_five__probe_semaphore = 0;
unsigned short test__prov_six__probe_semaphore = 0;
unsigned short test__prov_seven__probe_semaphore = 0;
unsigned short test__prov_eight__probe_semaphore = 0;
unsigned short test__prov_nine__probe_semaphore = 0;
unsigned short test__prov_ten__probe_semaphore = 0;
unsigned short test__prov_eleven__probe_semaphore = 0;
unsigned short test__prov_twelve__probe_semaphore = 0;

int
main(int argc, char **argv)
{
	if (test__prov_zero__probe_semaphore)
		STAP_PROBE(test__prov, zero__probe);
	if (test__prov_one__probe_semaphore)
		STAP_PROBE1(test__prov, one__probe, argc);
	if (test__prov_two__probe_semaphore)
		STAP_PROBE2(test__prov, two__probe, 2, 3);
	if (test__prov_three__probe_semaphore)
		STAP_PROBE3(test__prov, three__probe, 4, 5, 7);
	if (test__prov_four__probe_semaphore)
		STAP_PROBE4(test__prov, four__probe, 7, 8, 9, 10);
	if (test__prov_five__probe_semaphore)
		STAP_PROBE5(test__prov, five__probe, 11, 12, 13, 14, 15);
	if (test__prov_six__probe_semaphore)
		STAP_PROBE6(test__prov, six__probe, 16, 17, 18, 19, 20, 21);
	if (test__prov_seven__probe_semaphore)
		STAP_PROBE7(test__prov, seven__probe, 22, 23, 24, 25, 26, 27, 28);
	if (test__prov_eight__probe_semaphore)
		STAP_PROBE8(test__prov, eight__probe, 29, 30, 31, 32, 33, 34, 35, 36);
	if (test__prov_nine__probe_semaphore)
		STAP_PROBE9(test__prov, nine__probe, 37, 38, 39, 40, 41, 42, 43, 44, 45);
	if (test__prov_ten__probe_semaphore) {
		printf("should not see this since ten probe is not enabled!\n");
		STAP_PROBE10(test__prov, ten__probe, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55);
		fflush(stdout);
	}
	if (test__prov_eleven__probe_semaphore)
		STAP_PROBE11(test__prov, eleven__probe, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66);
	if (test__prov_twelve__probe_semaphore)
		STAP_PROBE12(test__prov, twelve__probe, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78);
}
EOF

${CC} ${CFLAGS} -o test test.c
if [ $? -ne 0 ]; then
	echo "failed to compile test.c" >& 2
	exit 1
fi

$dtrace -c ./test -qs /dev/stdin <<EOF
test-prov\$target:::zero-probe
{
	printf("%s:%s:%s\n", probemod, probefunc, probename);
}

test-prov\$target:::one-probe
{
	printf("%s:%s:%s:%li\n", probemod, probefunc, probename, arg0);
}

test-prov\$target:::two-probe
{
	printf("%s:%s:%s:%li:%li\n", probemod, probefunc, probename, arg0, arg1);
}

test-prov\$target:::three-probe
{
	printf("%s:%s:%s:%li:%li:%li\n", probemod, probefunc, probename, arg0, arg1,
	       arg2);
}

test-prov\$target:::four-probe
{
	printf("%s:%s:%s:%li:%li:%li:%li\n", probemod, probefunc, probename, arg0, arg1,
	       arg2, arg3);
}

test-prov\$target:::five-probe
{
	printf("%s:%s:%s:%li:%li:%li:%li:%li\n", probemod, probefunc, probename,
	       arg0, arg1, arg2, arg3, arg4);
}

test-prov\$target:::six-probe
{
	printf("%s:%s:%s:%li:%li:%li:%li:%li:%li\n", probemod, probefunc, probename,
	       arg0, arg1, arg2, arg3, arg4, arg5);
}

test-prov\$target:::seven-probe
{
	printf("%s:%s:%s:%li:%li:%li:%li:%li:%li:%li\n", probemod, probefunc, probename,
	       arg0, arg1, arg2, arg3, arg4, arg5, arg6);
}

test-prov\$target:::eight-probe
{
	printf("%s:%s:%s:%li:%li:%li:%li:%li:%li:%li:%li\n", probemod, probefunc, probename,
	       arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
}

test-prov\$target:::nine-probe
{
	printf("%s:%s:%s:%li:%li:%li:%li:%li:%li:%li:%li:%li\n", probemod, probefunc, probename,
	       arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
}

test-prov\$target:::eleven-probe,
test-prov\$target:::twelve-probe
{
	printf("%s:%s:%s:%li:%li:%li:%li:%li:%li:%li:%li:%li:%li\n", probemod, probefunc, probename,
	       arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
}
EOF
status=$?

exit $status
