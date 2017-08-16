/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *    Multiple aggregations are supported
 *
 * SECTION: Aggregations/Normalization
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
	@func1[i % 5] = sum(i * 100);
	@func2[i % 5 + 1] = sum(i * 200);
	i++;
}

tick-100ms
/i == 20/
{
	printf("normalized data #1:\n");
	normalize(@func1, 5);
	printa(@func1);

	printf("\nnormalized data #2:\n");
	normalize(@func2, 5);
	exit(0);
}
