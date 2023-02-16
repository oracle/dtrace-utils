/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: It is possible to read data from userspace addresses.
 *
 * SECTION: Actions and Subroutines/copyinto()
 */
/* @@trigger: delaydie */

#pragma D option quiet

syscall::write:entry
/pid == $target/
{
	ptr = (char *)alloca(32);
	copyinto(arg1, 32, ptr);
	printf("%s char match\n", ptr[4] == 'y' ? "good" : "BAD");
	printf("'%s'", stringof(ptr));
	exit(0);
}

ERROR
{
	exit(1);
}
