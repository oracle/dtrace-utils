/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: dtv2 */
/* @@runtest-opts: $_pid */
/* @@trigger: pid-tst-fork */
/* @@trigger-timing: before */

/*
 * ASSERTION: make sure fork(2) is okay
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
/pid == child/
{
	printf("wrong pid (%d %d)", pid, child);
	exit(1);
}

proc:::exit
/pid == $1 || pid == child/
{
	out++;
	trace(pid);
}

proc:::exit
/out == 2/
{
	exit(0);
}


profile:::tick-4
/timestamp > timeout/
{
	trace("test timed out");
	exit(1);
}

