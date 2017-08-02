/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * To print errno for failed system calls and make sure it succeeds, and is
 * correct.
 *
 * SECTION: Variables/Built-in Variables
 */

/* @@skip: known erratic */

#pragma D option quiet
#pragma D option destructive

BEGIN
{
	parent = pid;
	system("cat /non/existant/file");
}

syscall::open:entry
{
	this->fn = copyinstr(arg0);
}

syscall::open:return
/this->fn == "/non/existant/file" && errno != 0/
{
	printf("OPEN FAILED with errno %d for %s\n", errno, this->fn);
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
