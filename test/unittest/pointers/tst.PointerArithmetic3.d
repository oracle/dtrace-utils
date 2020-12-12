/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Pointer arithmetic implicitly adjusts the underlying address by
 * multiplying or dividing the operands by the size of the type referenced
 * by the pointer.
 *
 * SECTION: Pointers and Arrays/Pointer Arithmetic
 *
 * NOTES:
 *
 */

#pragma D option quiet

int *x;
int *y;
int a[5];

BEGIN
{
	x = &a[0];
	y = &a[2];

	printf("y-x: %x\n", (int)(y-x));
	exit(0);
}

END
/(2 != (int)(y-x))/
{
	printf("Error");
	exit(1);
}
