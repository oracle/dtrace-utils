#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

dtrace=$1
DIRNAME="$tmpdir/copyoutstr.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

# Construct a C program that:
# - initializes a user buffer
# - passes that buffer to a syscall (where DTrace will overwrite the buffer)
# - checks the buffer
cat << EOF > main.c
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
int main(int c, char **v) {
	int i, fd = open("/dev/null", O_WRONLY);
	char s[64];

	/* initialize buffer */
	memset(s, '-', sizeof(s));
	s[sizeof(s) - 1] = '\0';

	/* write buffer to /dev/null (the D script will overwrite s) */
	write(fd, s, strlen(s));
	close(fd);

	/* turn NUL chars (except last one) into printable '|' */
	for (i = 0; i < sizeof(s) - 1; i++)
		if (s[i] == '\0')
			s[i] = '|';

	/* print buffer contents */
	printf("%s\n", s);
	return 0;
}
EOF

gcc main.c
if [ $? -ne 0 ]; then
	echo "compilation error"
	exit 1
fi

# $1 is copyoutstr() size
# $2 is DTrace options
# $3 is description of test
function mytest() {
	echo $3
	$dtrace $dt_flags $2 -qwn '
	syscall::write:entry
	/pid == $target/
	{
		s = "HELLO WORLD; YOU HAVE A LONG MESSAGE TO DELIVER";
		copyoutstr(s, arg1, '$1');
		exit(0);
	}' -c ./a.out
	if [ $? -ne 0 ]; then
		echo "DTrace error"
		exit 1
	fi
}

mytest  64       " "      "size is longer than the string; expect full string with terminating byte"
mytest  32       " "      "size is shorter than the string; expect 32 chars without terminating byte"

exit 0
