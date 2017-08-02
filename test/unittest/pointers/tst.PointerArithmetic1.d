/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
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

BEGIN
{
	printf("x: %x\n", (int) x);
	printf("x + 1: %x\n", (int) (x+1));
	printf("x + 2: %x\n", (int) (x+2));
	exit(0);
}

END
/(0 != (int) x) || (4 != (int) (x+1)) || (8 != (int) (x+2))/
{
	printf("Error");
	exit(1);
}
