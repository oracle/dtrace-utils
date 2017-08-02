/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  lquantize() step value should be a 16-bit quantity
 *
 * SECTION: Aggregations/Aggregations
 *
 */


BEGIN
{
	i = 0;
}

tick-10ms
/i < 1000/
{
	@a[i] = lquantize(i, 100, 1100, 200000 );
	i += 100;
}

tick-10ms
/i == 1000/
{
	exit(0);
}

