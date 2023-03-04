/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Positive quantize()/lquantize()/clear() test
 *
 * SECTION: Aggregations/Aggregations
 *
 * NOTES:
 *	Verifies that printing a clear()'d aggregation with an lquantize()
 *	or quantize() aggregation function doesn't cause problems.
 */

/* @@nosort */
/* @@trigger: periodic_output */

#pragma D option switchrate=20ms
#pragma D option aggrate=1ms
#pragma D option quiet

BEGIN
{
	i = 0;
	n = 0;
}

syscall::write:entry
/pid == $target && (n % 2) == 0/
{
	i++; @a["linear"] = lquantize(i, 0, 100, 1); @b["exp"] = quantize(i);
	i++; @a["linear"] = lquantize(i, 0, 100, 1); @b["exp"] = quantize(i);
	i++; @a["linear"] = lquantize(i, 0, 100, 1); @b["exp"] = quantize(i);
	i++; @a["linear"] = lquantize(i, 0, 100, 1); @b["exp"] = quantize(i);
	i++; @a["linear"] = lquantize(i, 0, 100, 1); @b["exp"] = quantize(i);
	printa(@a);
	printa(@b);
	clear(@a);
	clear(@b);
}

syscall::write:entry
/pid == $target && n++ >= 8/
{
	exit(0);
}
