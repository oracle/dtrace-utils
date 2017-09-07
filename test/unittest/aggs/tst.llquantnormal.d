/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  normalize() works correctly for an llquantize() aggregation.
 *
 * SECTION: Aggregations/Aggregations
 *
 */

#pragma D option quiet

tick-10ms
/i++ < 10/
{
	@ = llquantize(i, 10, 0, 2, 20);
	@ = llquantize(i, 10, 0, 2, 20);
	@ = llquantize(i, 10, 0, 2, 20);
	@ = llquantize(i, 10, 0, 2, 20);
	@ = llquantize(i, 10, 0, 2, 20);
}

tick-10ms
/i == 10/
{
	exit(0);
}

END
{
	normalize(@, 5);
	printa(@);
}
