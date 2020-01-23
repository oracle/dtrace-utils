/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION: Pointer declarations are global.
 *
 * SECTION: Pointers and Arrays/Pointers and Addresses
 *
 * NOTES:
 *
 */

#pragma D option quiet

BEGIN
{
	i = 0;
	pfnAddress = &`max_pfn;
	pfnValue = *pfnAddress;
	printf("Address of max_pfn: %x\n", (int) pfnAddress);
	printf("Value of max_pfn: %d\n", pfnValue);
}

profile:::tick-1sec
/(i < 1) && (&`max_pfn == pfnAddress) && (*pfnAddress == pfnValue)/
{
	exit(1);
}

END
/(&`max_pfn == pfnAddress) && (*pfnAddress == pfnValue)/
{
	exit(0);
}
