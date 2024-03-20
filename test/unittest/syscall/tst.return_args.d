/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: bogus-ioctl */
/* @@trigger-timing: before */
/* @@runtest-opts: $_pid */

#pragma D option destructive
#pragma D option quiet

BEGIN
{
	n = 0;
}

syscall::ioctl:entry
/pid == $1 && !signalled/
{
	raise(SIGUSR1);
	signalled = 1;
}

syscall::open*:return
/pid == $1/
{
	n++;
	printf("%d %d\n", arg0, arg1);
}

syscall::open*:return
/pid == $1 && n >= 5/
{
	exit(0);
}
