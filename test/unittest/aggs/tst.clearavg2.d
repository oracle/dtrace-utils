/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Positive avg() test
 *
 * SECTION: Aggregations/Aggregations
 *
 * NOTES:
 *	Verifies that printing a clear()'d aggregation with an avg()
 *	aggregation function of 0 doesn't cause divide-by-zero problems.
 */
/* @@nosort */
/* @@trigger: periodic_output */

#pragma D option quiet
#pragma D option switchrate=50ms
#pragma D option aggrate=1ms

BEGIN
{
	@time = avg(0);
	printa(" %@d\n", @time);
	clear(@time);
	n = 0;
}

syscall::write:entry
/pid == $target && n == 2/
{
	printa(" %@d\n", @time);
}

syscall::write:entry
/pid == $target && n++ == 4/
{
	@time = avg(0);
	exit(0);
}
