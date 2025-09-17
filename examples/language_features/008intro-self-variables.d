/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -s

/*
 *  SYNOPSIS
 *    sudo ./008intro-self-variables.d
 *
 *  DESCRIPTION
 *    In D, we can also have variables that are local to a thread.
 *    We preface them with "self->".
 */

/* Thread-local "self" variables can be declared explicitly */
self int exp, dummy;

syscall::write:entry
{
	/* we initialize one variable to the fd of the write() call */
	self->exp = arg0;

	/* we can declare a self->variable implicitly, setting it to the requested number of bytes */
	self->imp = arg2;
}

fbt::ksys_write:entry
/ self->exp /
{
	self->kernel_value = arg2;
}

/*
 * We look for a write() return probe on the same thread where
 * self->exp has been set.  Using thread-local variables allows
 * us to connect two probes for the same thread, without interference
 * from other threads.
 */
syscall::write:return
/ self->exp /
{
	printf("on fd %d\n", self->exp);
	printf("bytes (requested): %d\n", self->imp);
	printf("bytes (requested in kernel function): %d\n", self->kernel_value);
	printf("bytes (actual): %d\n", arg1);

	/* uninitialized self-> variables are 0 */
	printf("uninitialized is zero: %d\n", self->dummy);

	/*
	 * It is good practice to zero out variables when you are
	 * done with them to free up the associated memory.
	 */
	self->exp = 0;
	self->imp = 0;
	self->kernel_value = 0;

	/* but for this example, we are finished */
	exit(0);
}
