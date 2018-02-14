/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2018, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@runtest-opts: $_pid */
/* @@trigger: pid-tst-vfork */
/* @@trigger-timing: before */

/*
 * ASSERTION: make sure probes called from a vfork(2) child fire in the parent
 *
 * SECTION: pid provider
 */

#pragma D option destructive

syscall::ioctl:return
/pid == $1/
{
	raise(SIGUSR1);

	timeout = timestamp + 500000000;
}

proc:::create
/pid == $1/
{
	child = args[0]->pr_pid;
	trace(pid);
}

pid$1:a.out:go:
/child != pid/
{
	printf("wrong pid (%d %d)", pid, child);
	exit(1);
}

syscall::exit_group:entry
/pid == $1/
{
	exit(0);
}
