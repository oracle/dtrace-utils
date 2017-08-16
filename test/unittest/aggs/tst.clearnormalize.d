/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Normalized aggregation data can be cleared
 *
 * SECTION: Aggregations/Normalization;
 *	Aggregations/Clearing aggregations
 *
 */

#pragma D option quiet
#pragma D option aggrate=1ms
#pragma D option switchrate=50ms

BEGIN
{
	i = 0;
	start = timestamp;
}

tick-100ms
/i < 20/
{
	@func[i % 5] = sum(i * 100);
	i++;
}

tick-100ms
/i == 10/
{
	printf("Normalized data before clear:\n");
	normalize(@func, 5);
	printa(@func);

	clear(@func);

	printf("Aggregation data after clear:\n");
	printa(@func);
	i++
}

tick-100ms
/i == 20/
{
	printf("Final (normalized) aggregation data:\n");
	normalize(@func, 5);
	printa(@func);

	exit(0);
}
