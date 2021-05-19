/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test a variety of fixed and dynamic format precisions.
 *
 * SECTION: Output Formatting/printf()
 */

#pragma D option quiet

BEGIN
{
	printf("\n");
	x = 0;

	printf("%.0s\n", "hello");
	printf("%.1s\n", "hello");
	printf("%.2s\n", "hello");
	printf("%.3s\n", "hello");
	printf("%.4s\n", "hello");
	printf("%.5s\n", "hello");

	printf("%.*s\n", x++, "hello");
	printf("%.*s\n", x++, "hello");
	printf("%.*s\n", x++, "hello");
	printf("%.*s\n", x++, "hello");
	printf("%.*s\n", x++, "hello");
	printf("%.*s\n", x++, "hello");

	exit(0);
}
