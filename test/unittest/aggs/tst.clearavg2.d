/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */
/*
 * ASSERTION:
 * 	Positive avg() test
 *
 * SECTION: Aggregations/Aggregations
 *
 * NOTES:
 *	Verifies that printing a clear()'d aggregation with an avg()
 *	aggregation function of 0 doesn't cause divide-by-zero problems.
 *
 */

#pragma D option quiet
#pragma D option switchrate=50ms
#pragma D option aggrate=1ms

tick-100ms
/(x++ % 5) == 0/
{
	@time = avg(0);
}

tick-100ms
/x > 5 && x <= 20/
{
	printa(" %@d\n", @time);
	clear(@time);
}

tick-100ms
/x > 20/
{
	exit(0);
}
