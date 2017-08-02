/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Positive avg() test
 *
 * SECTION: Aggregations/Aggregations
 *
 * NOTES:
 *	Verifies that printing a clear()'d aggregation with an avg()
 *	aggregation function doesn't cause problems.
 *
 */

#pragma D option quiet

tick-10ms
/i++ < 5/
{
	@a = avg(timestamp);
}

tick-10ms
/i == 5/
{
	exit(2);
}

END
{
	clear(@a);
	exit(0);
}
