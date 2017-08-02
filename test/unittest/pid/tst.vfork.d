/*
 * Oracle Linux DTrace.
 * Copyright © 2007, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@runtest-opts: $_pid */
/* @@trigger: pid-tst-vfork */
/* @@trigger-timing: after */

/*
 * ASSERTION: make sure probes called from a vfork(2) child fire in the parent
 *
 * SECTION: pid provider
 */

#pragma D option destructive

pid$1:a.out:waiting:entry
{
	this->value = (int *)alloca(sizeof (int));
	*this->value = 1;
	copyout(this->value, arg0, sizeof (int));
}

proc:::create
/pid == $1/
{
	child = args[0]->pr_pid;
}

pid$1:a.out:go:
/child != pid/
{
	printf("wrong pid (%d %d)", pid, child);
	exit(1);
}

syscall::rexit:entry
/pid == $1/
{
	exit(0);
}
