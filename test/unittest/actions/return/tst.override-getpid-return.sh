#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

# Set up test directory.

DIRNAME=$tmpdir/override-getpid.$$.$RANDOM
mkdir -p $DIRNAME
cd $DIRNAME

# Make the trigger.  It reports the pid 10 times.

cat << EOF > main.c
#include <stdio.h>
#include <unistd.h>

int main(int c, char **v) {
	int i;

	for (i = 1; i <= 10; i++)
		printf("%02d %d\n", i, getpid());

	return 0;
}
EOF

$CC main.c
if [ $? -ne 0 ]; then
	echo ERROR: compile
	exit 1
fi

# Trace the trigger.  On the 2nd and 5th getpid() calls, modify the result.

$dtrace $dt_flags -c ./a.out -w -q -n '
BEGIN {
	printf("00 pid is %d\n", $target);
	n = 0;
}
rawfbt:vmlinux:__*_sys_getpid:return
/pid == $target/
{
	n++;
}
rawfbt:vmlinux:__*_sys_getpid:return
/pid == $target && n == 2/
{
	return(22222);
}
rawfbt:vmlinux:__*_sys_getpid:return
/pid == $target && n == 5/
{
	return(55555);
}
' | sort

exit $?
