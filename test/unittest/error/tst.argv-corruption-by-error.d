/*
 * Oracle Linux DTrace.
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: A fault that triggers the ERROR probe terminates the execution of
 *            the current clause, while other clauses for the same probe should
 *            still be executed.  This tests that the ERROR probe invocation
 *            does not corrupt the arguments of the original probe.
 *
 * SECTION: dtrace Provider
 */

#pragma D option quiet

syscall::write*:entry
{
	self->arg0 = arg0;
	self->arg1 = arg1;
	self->arg2 = arg2;
	self->arg3 = arg3;
	self->arg4 = arg4;
	self->arg5 = arg5;

	printf("%d / %d / %d / %d / %d / %d\n",
	       arg0, arg1, arg2, arg3, arg4, arg5);
}

syscall::write*:entry
{
	trace(*(int *)0);
}

syscall::write*:entry,
ERROR {
	printf("%d / %d / %d / %d / %d / %d\n",
	       arg0, arg1, arg2, arg3, arg4, arg5);
}

syscall::write*:entry
{
	exit(self->arg0 != arg0 || self->arg1 != arg1 || self->arg2 != arg2 ||
	     self->arg3 != arg3 || self->arg4 != arg4 || self->arg5 != arg5
		? 1 : 0);
}
