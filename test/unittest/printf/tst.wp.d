/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test a variety of fixed and dynamic format widths combined with precisions.
 *
 * SECTION: Output Formatting/printf()
 */

#pragma D option quiet

BEGIN
{
	printf("\n");
	x = 0;

	printf("%0.0s\n", "hello");
	printf("%1.1s\n", "hello");
	printf("%2.2s\n", "hello");
	printf("%3.3s\n", "hello");
	printf("%4.4s\n", "hello");
	printf("%5.5s\n", "hello");
	printf("%6.6s\n", "hello");
	printf("%7.7s\n", "hello");
	printf("%8.8s\n", "hello");
	printf("%9.9s\n", "hello");

	printf("%*.*s\n", x, x++, "hello");
	printf("%*.*s\n", x, x++, "hello");
	printf("%*.*s\n", x, x++, "hello");
	printf("%*.*s\n", x, x++, "hello");
	printf("%*.*s\n", x, x++, "hello");
	printf("%*.*s\n", x, x++, "hello");
	printf("%*.*s\n", x, x++, "hello");
	printf("%*.*s\n", x, x++, "hello");
	printf("%*.*s\n", x, x++, "hello");
	printf("%*.*s\n", x, x++, "hello");

	exit(0);
}
