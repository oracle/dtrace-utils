/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Number of quantization levels must be a 16-bit quantity
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
	@a[i] = lquantize(i, 0, 1000000, 10);
	i += 100;
}

tick-10ms
/i == 1000/
{
	exit(0);
}

