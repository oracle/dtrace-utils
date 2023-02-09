/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Positive lquantize()/clear() test
 *
 * SECTION: Aggregations/Aggregations
 *
 * NOTES:
 *	Verifies that printing a clear()'d aggregation with an lquantize()
 *	aggregation function doesn't cause problems.
 *
 */


#pragma D option switchrate=50ms
#pragma D option aggrate=1ms
#pragma D option quiet

tick-100ms
{
	x++;
	@a["linear"] = lquantize(x, 0, 100, 1);
	@b["exp"] = quantize(x);
}

tick-100ms
/(x % 5) == 0 && y++ < 5/
{
	printa(@a);
	printa(@b);
	clear(@a);
	clear(@b);
}

tick-100ms
/(x % 5) == 0 && y == 5/
{
	exit(0);
}
