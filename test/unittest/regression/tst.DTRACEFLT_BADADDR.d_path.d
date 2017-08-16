/*
 * Oracle Linux DTrace.
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Ensure DTRACEFLT_BADADDR is reported when d_path() is called with
 *	an invalid address.
 *
 * SECTION: dtrace Provider
 */

#pragma D option quiet

ERROR
{
	printf("The arguments are %u %u %u %u %u\n",
		arg1, arg2, arg3, arg4, arg5);
	printf("The value of arg4 should be %u\n", DTRACEFLT_BADADDR);
	printf("The value of arg5 should be %u\n", 0x18);
	exit(0);
}

BEGIN
{
	d = d_path((struct path *)0x18);
	trace(d);
}
