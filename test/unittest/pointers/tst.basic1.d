/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Pointers can be stored in variables.
 *
 * SECTION: Pointers and Arrays/Pointers and Addresses
 *
 * NOTES:
 *
 */

#pragma D option quiet

BEGIN
{
	pfnAddress = (unsigned long *)&`max_pfn;
	pfnValue = *pfnAddress;
	printf("Address of max_pfn: %x\n", (int)pfnAddress);
	printf("Value of max_pfn: %d\n", pfnValue);
	exit(0);
}
