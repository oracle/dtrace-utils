/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: It is possible to read data from userspace addresses.
 *
 * SECTION: Actions and Subroutines/copyinto()
 */

#pragma D option quiet
#pragma D option destructive

BEGIN
{
	system("echo dtrace-copyinto-test");
}

syscall::write:entry
{
	ptr = (char *)alloca(32);
	copyinto(arg1, 32, ptr);
	ok = ptr[6] == '-';
}

syscall::write:entry
/ok/
{
	ptr = (char *)alloca(32);
	copyinto(arg1, 32, ptr);
	printf("'%s'", stringof(ptr));
	exit(0);
}

ERROR
{
	exit(1);
}
