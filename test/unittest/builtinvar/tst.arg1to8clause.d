/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * print arg1 to arg8 from a profile and make sure it succeeds
 *
 * SECTION: Variables/Built-in Variables
 */

#pragma D option quiet

BEGIN
{
}

syscall:::entry
{
	printf("The argument is %u %u %u %u %u %u %u %u\n", arg1, arg2,
		arg3, arg4, arg5, arg6, arg7, arg8);
	exit(0);
}
