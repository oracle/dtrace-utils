/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: bogus-ioctl */
/* @@trigger-timing: before */
/* @@runtest-opts: $_pid */

#pragma D option destructive
#pragma D option quiet
#pragma D option zdefs

BEGIN
{
	niter = 0;
	rval = -1;
}

/* notify the trigger to exit its ioctl() loop */
syscall::ioctl:entry
/pid == $1/
{
	raise(SIGUSR1);
}

/* if we enter open(), reset the expected return value */
syscall::open*:entry
/pid == $1/
{
	reset = 1;
}

/* check the return value (the first probe resets the expected value) */
syscall:vmlinux:open*:return,
fbt:vmlinux:do_sys_open*:return
/pid == $1/
{
	rval = (reset ? arg1 : rval);
	printf("%s", arg1 == rval ? "" : "ERROR mismatch\n");
	reset = 0;
}

syscall::open*:return
/pid == $1 && ++niter >= 20/
{
	exit(0);
}
