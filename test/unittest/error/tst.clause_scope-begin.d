/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: A fault that triggers the ERROR probe terminates the execution of
 *            the current clause, but other clauses for the same probe should
 *            still be executed.  This tests the special case where the fault
 *            occurs in the BEGIN probe and the probe data is processed before
 *            probing ends.
 *
 * SECTION: dtrace Provider
 */

#pragma D option quiet

ERROR
{
	printf("Error fired\n");
}

BEGIN
{
	trace(((struct task_struct *)NULL)->pid);
}

BEGIN
{
	printf("Clause executed\n");
}

tick-1s
{
	exit(0);
}
