/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Upper bound must be greater than lower bound argument
 *
 * SECTION: Aggregations/Aggregations
 *
 */

#pragma D option quiet

BEGIN
{
	i = 0;
}

tick-1
/i < 1000/
{
	@a[i] = lquantize(i, 0, 1100, -100);
	i += 100;
}

tick-1
/i == 1000/
{
	exit(0);
}
