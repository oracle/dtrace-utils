/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2023, Oracle and/or its affiliates. All rights reserved.
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
 *	aggregation function doesn't cause problems.
 */
/* @@nosort */
/* @@trigger: periodic_output */

#pragma D option quiet

BEGIN
{
	@a = avg(timestamp);
	@a = avg(timestamp);
	@a = avg(timestamp);
	@a = avg(timestamp);
	@a = avg(timestamp);
	clear(@a);
	n = 0;
}

syscall::write:entry
/pid == $target && n++ == 2/
{
	exit(0);
}
