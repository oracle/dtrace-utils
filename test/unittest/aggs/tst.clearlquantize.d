/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Positive quantize()/lquantize()/clear() test
 *
 * SECTION: Aggregations/Aggregations
 *
 * NOTES:
 *	Verifies that printing a clear()'d aggregation with an lquantize()
 *	or quantize() aggregation function doesn't cause problems.
 */

/* @@nosort */

#pragma D option switchrate=20ms
#pragma D option aggrate=1ms
#pragma D option quiet

BEGIN
{
	z = 0;
}

tick-500ms
/(z % 2) == 0/
{
	x++; @a["linear"] = lquantize(x, 0, 100, 1); @b["exp"] = quantize(x);
	x++; @a["linear"] = lquantize(x, 0, 100, 1); @b["exp"] = quantize(x);
	x++; @a["linear"] = lquantize(x, 0, 100, 1); @b["exp"] = quantize(x);
	x++; @a["linear"] = lquantize(x, 0, 100, 1); @b["exp"] = quantize(x);
	x++; @a["linear"] = lquantize(x, 0, 100, 1); @b["exp"] = quantize(x);
	printa(@a);
	printa(@b);
}

tick-500ms
/(z % 2) == 1/
{
	clear(@a);
	clear(@b);
}

tick-500ms
/z++ >= 9/
{
	exit(0);
}
