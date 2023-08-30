/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Loading from array elements works for alloca pointers.
 *
 * SECTION: Actions and Subroutines/alloca()
 */

#pragma D option quiet

int *arr;

BEGIN
{
	arr = alloca(5 * sizeof(int));
	arr[3] = 0x42;
	trace(arr[3]);
	exit(0);
}

ERROR
{
	exit(1);
}
