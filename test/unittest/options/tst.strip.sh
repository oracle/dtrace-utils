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

DIRNAME="$tmpdir/strip.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

# set up the prov.d file and compile

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

# set up the test.c file and compile

cat > test.c <<EOF
#include <sys/types.h>
#include "prov.h"
int main(int argc, char **argv)
{
	TEST_PROV_GO();
	return 0;
}
EOF

${CC} ${test_cppflags} ${test_ldflags} ${CFLAGS} -c test.c
if [ $? -ne 0 ]; then
	echo "failed to compile test.c" >& 2
	exit 1
fi

# link with and without -xstrip, dumping the DOF section

objdump="objdump --full-contents --section=.SUNW_dof prov.o"

$dtrace $dt_flags -G -xstrip -s prov.d test.o
if [ $? -ne 0 ]; then
	echo "failed to create DOF (stripped)" >& 2
	exit 1
fi
$objdump >& out.stripped.txt

$dtrace $dt_flags -G -s prov.d test.o
if [ $? -ne 0 ]; then
	echo "failed to create DOF" >& 2
	exit 1
fi
$objdump >& out.default.txt

# Check results.  One can imagine a more stringent check -- like
# seeing if the string "Oracle D 2.0" appears in the default case
# but not in the stripped case -- but here we settle for the stripped
# case simply being smaller than the default case.

nbytes_stripped=`wc -c out.stripped.txt | gawk '{print $1}'`
nbytes_default=`wc -c out.default.txt | gawk '{print $1}'`

echo "number of bytes:"
echo "    stripped: $nbytes_stripped"
echo "    default: $nbytes_default"

if [ $nbytes_stripped -eq 0 -o $nbytes_default -eq 0 ]; then
	echo ERROR: a number of bytes is zero
	exit 1
fi
if [ $nbytes_stripped -ge $nbytes_default ]; then
	echo ERROR: stripped is greater or equal to default
	exit 1
fi

echo success: stripped is smaller than default
exit 0
