#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1
CC=/usr/bin/gcc
CFLAGS=

DIRNAME="$tmpdir/arrays-uregsarray-check.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

# compile USDT provider

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

# compile instrumented user test program

cat > test.c <<EOF
#include <stdio.h>

#include <sys/types.h>
#include "prov.h"

int main(int c, char **v) {
    int local_variable = 10;

    printf("instructions start at %p\n", &main);
    printf("local variable at %p\n", &local_variable);
    TEST_PROV_GO();
    return 0;
}
EOF

${CC} ${test_cppflags} ${CFLAGS} -c test.c
if [ $? -ne 0 ]; then
	echo "failed to compile test.c" >& 2
	exit 1
fi
$dtrace $dt_flags -G -s prov.d test.o
if [ $? -ne 0 ]; then
	echo "failed to create DOF" >& 2
	exit 1
fi
${CC} ${test_ldflags} ${CFLAGS} -o test test.o prov.o
if [ $? -ne 0 ]; then
	echo "failed to link final executable" >& 2
	exit 1
fi

# run DTrace

$dtrace $dt_flags -c ./test -qn '
test_prov$target:::
{
	printf("DTrace has PC %x and SP %x\n", uregs[R_PC], uregs[R_SP]);
	exit(0);
}' -o D.out > C.out

if [ $? -ne 0 ]; then
	echo DTrace ERROR
	cat D.out C.out
	exit 1
fi

# post processing

gawk '
BEGIN { DTrace_PC = DTrace_SP = instructions = local_variable = -1 }

# the first file has DTrace output
NR == FNR && /^DTrace has PC / { DTrace_PC = "0x"$4; DTrace_SP = "0x"$7; next; }

# the second file has output from the C program
NR != FNR && /^instructions start at / { instructions = $4; next; }
NR != FNR && /^local variable at /   { local_variable = $4; next; }

END {
	if ( DTrace_PC <= 0 ||
	     DTrace_SP <= 0 ||
	     instructions <= 0 ||
	     local_variable <= 0 ) {
		print "ERROR: did not see all output in post-processing";
		exit 1;
	}

	# the PC must be early in the reported function
	offset = strtonum(DTrace_PC) - strtonum(instructions);
	print "PC offset is", DTrace_PC, " -", instructions, "=", offset;
	if ( offset < 0 || offset > 100 ) {
		print "ERROR: instruction offset is unreasonable";
		exit 1;
	}

	# the local variable address must be a little higher than the SP
	offset = strtonum(local_variable) - strtonum(DTrace_SP);
	print "stack variable offset is", local_variable, " -", DTrace_SP, "=", offset;
	if ( offset < 0 || offset > 100 ) {
		print "ERROR: stack variable offset is unreasonable";
		exit 1;
	}

	print "success: both offsets are reasonable";
	exit 0;
}' D.out C.out

exit $?
