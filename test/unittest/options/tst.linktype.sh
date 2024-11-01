#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

# @@nosort

dtrace=$1
CC=/usr/bin/gcc
CFLAGS=

DIRNAME="$tmpdir/linktype.$$.$RANDOM"
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

# link with different -xlinktype values

function mytest() {
	$dtrace $dt_flags -G $1 -s prov.d test.o

	# report whether the link succeeded
	if [ $? -ne 0 ]; then
		echo "link FAIL:" $1
	else
		echo "link pass:" $1
	fi

	# report whether the file format is recognized
	objdump --file-headers prov.o |& gawk '
	    /format not recognized/ {
		print "objdump does NOT recognize file format";
		exit(0);
	    }
	    /file format elf/ {
		print "objdump recognizes elf";
		exit(0);
	    }'
}

mytest " "              # link should pass, file format should be recognized
mytest -xlinktype=elf   # link should pass, file format should be recognized
mytest -xlinktype=dof   # link should pass, file format should NOT be recognized
mytest -xlinktype=foo   # link should FAIL, file format should NOT be recognized

exit 0
