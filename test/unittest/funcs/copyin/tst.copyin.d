/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: It is possible to read data from userspace addresses.
 *
 * SECTION: Actions and Subroutines/copyin()
 *	    User Process Tracing/copyin() and copyinstr() Subroutines
 */
/* @@trigger: delaydie */

#pragma D option quiet

syscall::write:entry
/pid == $target/
{
	printf("%s char match\n", (s = (string)copyin(arg1, 32))[4] == 'y' ? "good" : "BAD");
	printf("'%s'", s);
	exit(0);
}

ERROR
{
	exit(1);
}
