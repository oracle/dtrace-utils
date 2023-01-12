#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

dtrace=$1
DIRNAME="$tmpdir/copyout.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

cat << EOF > main.c
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
int main(int c, char **v) {
	int fd = open("/dev/null", O_WRONLY);
	char s[256];

	/* the user buffer is filled with a lowercase message */
	sprintf(s, "hello world; you have a long message to deliver");

	/* DTrace will intercept this call and overwrite the user buffer */
	write(fd, s, strlen(s));

	close(fd);
	printf("%s\n", s);
	return 0;
}
EOF

gcc main.c
if [ $? -ne 0 ]; then
	echo "compilation error"
	exit 1
fi

$dtrace $dt_flags -qwn '
syscall::write:entry
/pid == $target/
{
	s = "HELLO WORLD; YOU HAVE A LONG MESSAGE TO DELIVER";
	copyout(s, arg1, 32);
	exit(0);
}' -c ./a.out

if [ $? -ne 0 ]; then
	echo "DTrace error"
	exit 1
fi

exit 0
