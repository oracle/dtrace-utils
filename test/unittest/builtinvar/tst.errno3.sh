#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1
CC=/usr/bin/gcc

DIRNAME="$tmpdir/builtinvar-errno3.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

cat << EOF > main.c
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>  /* open() */
#include <sys/stat.h>   /* open() */
#include <fcntl.h>      /* open() */
#include <unistd.h>     /* close() */
void foo(char *s) {
    int fd = open(s, O_WRONLY);
    close(fd);
}
int main(int c, char **v) {
    foo("/dev/null");
    foo("/dev/null");
    foo("/no/such/path/exists");
    foo("/no/such/path/exists");
    foo("/dev/null");
    foo("/dev/null");
    return 0;
}
EOF

$CC $test_cppflags main.c
if [ $? -ne 0 ]; then
    echo compilation failed
    exit 1
fi

$dtrace $dt_flags -Zqn '
    syscall::open:,
    syscall::openat:,
    syscall::close:
    /pid == $target/
    { printf(" %d", errno); }
' -c ./a.out
if [ $? -ne 0 ]; then
    echo DTrace failed
    exit 1
fi

exit 0
