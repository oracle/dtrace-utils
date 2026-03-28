#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

dtrace=$1

DIRNAME="$tmpdir/progenyof2.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

# make the trigger

cat << EOF > a.c
int main(void) {
	return 0;
}
EOF
$CC $test_cppflags $test_ldflags a.c
if [ $? -ne 0 ]; then
	echo ERROR: compiling trigger
	exit 1
fi

# check that progenyof(ppid) and progenyof(pid) are nonzero

$dtrace -c ./a.out -qn 'pid$target:a.out:main:* { exit((progenyof(ppid) && progenyof(pid)) ? 0 : 1) }'

exit $?
