/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: It is possible to read data from userspace addresses.
 *
 * SECTION: Actions and Subroutines/copyin()
 *	    User Process Tracing/copyin() and copyinstr() Subroutines
 */

#pragma D option quiet
#pragma D option destructive

BEGIN
{
	system("echo dtrace-copyin-test");
}

syscall::write:entry
/(s = (string)copyin(arg1, 32))[6] == '-'/
{
	printf("'%s'", s);
	exit(0);
}

ERROR
{
	exit(1);
}
