/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * print arg0 from a profile and make sure it succeeds
 *
 * SECTION: Variables/Built-in Variables
 */

#pragma D option quiet

BEGIN
{
	printf("Call probe read\n");
}

syscall:::entry
{
	printf("The argument is %u", arg0);
	exit(0);
}
