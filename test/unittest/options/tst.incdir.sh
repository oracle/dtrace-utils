#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

DIRNAME="$tmpdir/incdir.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

# form a D script that relies on a header file

cat << EOF > D.d
#include "D.h"
BEGIN
{
	printf("%c%c%c%c\n", (foo) 0x67, (foo) 0x6f, (foo) 0x6f, (foo) 0x64);
	exit(0);
}
EOF

# hide the header file in a subdirectory

mkdir mysubdir
cat << EOF > mysubdir/D.h
typedef char foo;
EOF

# confirm we do not find the subdirectory by default

$dtrace $dt_flags -Cqs D.d
if [ $? -eq 0 ]; then
	echo ERROR: script should not pass by default
	exit 1
else
	echo script fails by default as expected
fi

# confirm that -xincdir fixes the problem

$dtrace $dt_flags -xincdir=mysubdir -Cqs D.d
if [ $? -eq 0 ]; then
	echo script runs with incdir
	exit 0
else
	echo ERROR: script fails even with incdir
	exit 1
fi
