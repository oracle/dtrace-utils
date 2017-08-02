/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2012, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Pointers can be stored in associative arrays.
 *
 * SECTION: Pointers and Arrays/Pointers and Addresses
 *
 * NOTES:
 *
 */

#pragma D option quiet

BEGIN
{
	assoc_array["pfnAddress"] = &`max_pfn;
	pfnValue = *(assoc_array["pfnAddress"]);
	printf("Address of max_pfn: %x\n", (int) assoc_array["pfnAddress"]);
	printf("Value of max_pfn: %d\n", pfnValue);
	exit(0);
}
