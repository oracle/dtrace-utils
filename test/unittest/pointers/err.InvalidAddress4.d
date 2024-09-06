/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Demonstrating valid memory access.
 *
 * SECTION: Pointers and Arrays/Pointers and Addresses
 *
 * NOTES:
 *
 */

#pragma D option quiet

BEGIN
{
	x = (int *)alloca(sizeof(int));
	printf("Address x: %x\n", (int)x);
	y = (int *)(x - 2);
	*y = 3;
	printf("Address y: %x\tValue: %d\n", (int)y, *y);
}

ERROR
{
	exit(1);
}
