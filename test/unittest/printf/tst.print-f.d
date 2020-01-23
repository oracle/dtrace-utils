/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 *  Test %f format printing.
 *
 * SECTION: Output Formatting/printf()
 *
 */

#pragma D option quiet

float f;
double d;

BEGIN
{
	printf("\n");

	printf("%%f = %f\n", f);
	printf("%%f = %f\n", d);


	exit(0);
}
