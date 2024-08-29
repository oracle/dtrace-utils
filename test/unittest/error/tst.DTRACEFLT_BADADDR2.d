/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	To test DTRACEFLT_BADADDR error with non-NULL address
 *
 * SECTION: dtrace Provider
 */

#pragma D option quiet

ERROR
{
	printf("The arguments are %u %u %u %u\n",
		arg1, arg2, arg4, arg5);
	printf("The value of arg4 should be %u\n", DTRACEFLT_BADADDR);
	printf("The value of arg5 should be %u\n", 0x4000);
	exit(0);
}

BEGIN
{
	x = (int *)0x4000;
	y = *x;
	trace(y);
}
