/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Positive lquantize() test
 *
 * SECTION: Aggregations/Aggregations
 *
 * NOTES: This is verifiable simple positive test of the lquantize() function.
 *
 */

#pragma D option quiet

BEGIN
{
	i = 0;
}

tick-10ms
/i < 1000/
{
	@a[i] = lquantize(i, -100, 1100, 100);
	i += 100;
}

tick-10ms
/i == 1000/
{
	exit(0);
}
