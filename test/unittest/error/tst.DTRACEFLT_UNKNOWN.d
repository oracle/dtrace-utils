/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	To test DTRACEFLT_UNKNOWN error
 *
 * SECTION: dtrace Provider
 *
 */


#pragma D option quiet

ERROR
{
	printf("The arguments are %u %u %u %u %u\n",
		arg1, arg2, arg3, arg4, arg5);
	printf("The value of arg4 = %u\n", DTRACEFLT_UNKNOWN);
	exit(0);
}

BEGIN
{
	x = (int *) 64;
	y = *x;
	trace(y);
}
