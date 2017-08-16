/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test the use of tuple arguments in the printa() format.
 *
 * SECTION: Output Formatting/printa()
 *
 * NOTES:
 *  We confirm that we can consume fewer arguments than in the tuple, all
 *  the way up to the exact number.
 */

#pragma D option quiet

BEGIN
{
	@a[1, 2, 3, 4, 5] = count();
	printf("\n");

	printa("%@u: -\n", @a);
	printa("%@u: %d\n", @a);
	printa("%@u: %d %d\n", @a);
	printa("%@u: %d %d %d\n", @a);
	printa("%@u: %d %d %d %d\n", @a);
	printa("%@u: %d %d %d %d %d\n", @a);

	exit(0);
}
