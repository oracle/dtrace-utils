/*
 * Oracle Linux DTrace.
 * Copyright © 2006, 2012, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: D pointers do not allow invalid pointer accesses.
 *
 * SECTION: Pointers and Arrays/Pointer Safety
 *
 * NOTES:
 *
 */

#pragma D option quiet

BEGIN
{
	pfnAddress = &`max_pfn;
	*pfnAddress = 20;
	printf("Address of pfnAddress: %d\n", (int) pfnAddress);
	printf("Value of pfnAddress: %d\n", `max_pfn);
	exit(0);
}

ERROR
{
	exit(1);
}
