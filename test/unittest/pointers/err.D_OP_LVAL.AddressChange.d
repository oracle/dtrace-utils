/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2012, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION: D pointers do not allow invalid address changes.
 *
 * SECTION: Pointers and Arrays/Pointer Safety
 *
 * NOTES:
 *
 */

#pragma D option quiet

BEGIN
{
	&`max_pfn = (int *) 0x185ede4;
	pfnAddress = &`max_pfn;
	printf("Address of max_pfn: %d\n", (int) pfnAddress);
	printf("Value of max_pfn: %d\n", `max_pfn);
}

ERROR
{
	exit(1);
}
