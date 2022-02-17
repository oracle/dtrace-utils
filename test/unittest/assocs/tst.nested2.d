/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Nested tuple assembly works correctly.
 *
 * SECTION: Variables/Associative Arrays
 */

#pragma D option quiet

BEGIN
{
	a[1, 2, 3] = 1234;
	a[3, 2, 3] = 4321;
	a[3, 2, 1] = 2;
	printf("%d\n", a[1, a[3, a[3, a[3, a[3, 2, 1], 1], 1], 1], 3]);

	exit(0);
}

ERROR
{
	exit(1);
}
