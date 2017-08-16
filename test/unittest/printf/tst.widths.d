/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test a variety of fixed and dynamic format widths.
 *
 * SECTION: Output Formatting/printf()
 *
 */

#pragma D option quiet

BEGIN
{
	printf("\n");
	x = 0;

	printf("%0d\n", 1);
	printf("%1d\n", 1);
	printf("%2d\n", 1);
	printf("%3d\n", 1);
	printf("%4d\n", 1);
	printf("%5d\n", 1);

	printf("%*d\n", x++, 1);
	printf("%*d\n", x++, 1);
	printf("%*d\n", x++, 1);
	printf("%*d\n", x++, 1);
	printf("%*d\n", x++, 1);
	printf("%*d\n", x++, 1);

	exit(0);
}
