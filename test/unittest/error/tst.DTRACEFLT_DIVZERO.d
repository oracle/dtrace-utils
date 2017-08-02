/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	To test DTRACEFLT_DIVZERO error
 *
 * SECTION: dtrace Provider
 *
 */


#pragma D option quiet

ERROR
{
	printf("The arguments are %u %u %u %d %u\n",
		arg1, arg2, arg3, arg4, arg5);
	exit(0);
}

char *s;

BEGIN
{
	i = 1;
	j = 2;
	j = i/(j-2);
}
