/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * 	Global variables defined once, is visible in every clause of
 *	D program
 *
 * SECTION:  Variables/Scalar Variables
 *
 */

#pragma D option quiet

int x;

BEGIN
{
	x = 123;
}

profile:::tick-1sec
{
	printf("The value of x is %d\n", x);
}

profile:::tick-100msec
{
	printf("The value of x is %d\n", x);
	exit (0);
}
