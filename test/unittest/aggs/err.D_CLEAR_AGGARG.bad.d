/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */


/*
 * ASSERTION:
 *	The argument to clear() must be an aggregation.
 *
 * SECTION: Aggregations/Clearing aggregations
 *
 *
 */

#pragma D option quiet
#pragma D option aggrate=10ms
#pragma D option switchrate=10ms

BEGIN
{
	i = 0;
	start = timestamp;
}

tick-100ms
/i < 20/
{
	@func[i%5] = sum(i * 100);
	i++;
}

tick-100ms
/i == 10/
{
	printf("Aggregation data before clear():\n");
	printa(@func);

	clear(count());

	printf("Aggregation data after clear():\n");
	printa(@func);
	i++;
}

tick-100ms
/i == 20/
{
	printf("Final aggregation data:\n");
	printa(@func);

	exit(0);
}
