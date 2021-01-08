/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 * To print errno for failed system calls and make sure it succeeds, and is
 * correct.
 *
 * SECTION: Variables/Built-in Variables
 */

#pragma D option quiet
#pragma D option destructive

BEGIN
{
	parent = pid;
	system("cat /non/existant/file");
}

/*
 * Use one clause for syscall::openat:entry and one for syscall::open:entry.
 * Record file name pointer arg1 for the 'openat' function and arg0 for 'open'.
 */
syscall::open:entry
{
	self->fn = arg0;  /* 'open' arg0 holds a pointer to the file name */
}

syscall::openat:entry
{
	self->fn = arg1;  /* 'openat' arg1 holds a pointer to the file name */
}

syscall::open*:return
/copyinstr(self->fn) == "/non/existant/file" && errno != 0/
{
	printf("OPEN FAILED with errno %d\n", errno);
}

proc:::exit
/progenyof(parent)/
{
	printf("At exit, errno = %d\n", errno);
}

proc:::exit
/progenyof(parent)/
{
	exit(0);
}

ERROR
{
	exit(1);
}
