#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This script tests that the proc:::exec probe fires for execve() and
# produces the correct probe arg.

dtrace=$1

DIRNAME="$tmpdir/exec-execve.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

cat << EOF > parent.c
#include <stdio.h>
#include <unistd.h>

int main(int c, char **v) {
  char *argv[] = { "bogus_exec", NULL };
  char *envp[] = { NULL };
  int rc;

  printf("exec\n");
  rc = execve("bogus_direc/bogus_exec", argv, envp);

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
syscall::execve:entry
/ppid == dtpid && execname == "parent.x"/
{
    printf("execve      %s\n", stringof(arg0));
}' -c ./parent.x
if [ $? -ne 0 ]; then
    echo ERROR
    exit 1
fi

exit 0
