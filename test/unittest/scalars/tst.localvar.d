/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 *	Assign a value to a variable in a local clause.
 *
 * SECTION:  Variables/Scalar Variables
 *
 */

#pragma D option quiet

BEGIN
{
}
int i;

profile:::tick-1sec
{
	i = 0;
}

profile:::tick-100msec
{
	printf("The value of int i is %d\n", i);
	exit (0);
}
