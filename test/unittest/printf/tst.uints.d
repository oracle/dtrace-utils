/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test printf() with simple unsigned integer arguments.
 *
 * SECTION: Output Formatting/printf()
 *
 */

#pragma D option quiet

BEGIN
{
	printf("\n");
	printf("%u\n", (uchar_t)123);
	printf("%u\n", (ushort_t)123);
	printf("%u\n", (ulong_t)123);
	printf("%u\n", (u_longlong_t)123);
	exit(0);
}
