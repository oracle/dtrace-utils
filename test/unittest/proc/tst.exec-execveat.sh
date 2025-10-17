#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This script tests that the proc:::exec probe fires for execveat() and
# produces the correct probe arg.

dtrace=$1

DIRNAME="$tmpdir/exec-execveat.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

cat << EOF > parent.c
#include <stdio.h>
#include <linux/fcntl.h>      /* Definition of AT_* constants */
#include <sys/syscall.h>      /* Definition of SYS_* constants */
#include <unistd.h>

int main(int c, char **v) {
  char *argv[] = { "bogus_exec", NULL };
  char *envp[] = { NULL };
  int rc;

  printf("exec\n");
  rc = syscall(__NR_execveat, AT_FDCWD, "bogus_direc/bogus_exec", argv, envp, 0);

  return 0;
}
EOF

${CC} -o parent.x parent.c

$dtrace $dt_flags -qn '
BEGIN { dtpid = pid; }
proc:::exec
/ppid == dtpid && execname == "parent.x"/
{
    printf("proc:::exec %s\n", args[0]);
}
syscall::execveat:entry
/ppid == dtpid && execname == "parent.x"/
{
    printf("execveat    %s\n", copyinstr(arg1));
}' -c ./parent.x
if [ $? -ne 0 ]; then
    echo ERROR
    exit 1
fi

exit 0
