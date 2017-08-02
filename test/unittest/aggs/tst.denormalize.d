/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Simple denormalization test
 *
 * SECTION: Aggregations/Normalization
 *
 */

#pragma D option quiet


BEGIN
{
	i = 0;
	start = timestamp;
}

tick-10ms
/i < 20/
{
	@func[i % 5] = sum(i * 100);
	i++;
}

tick-10ms
/i == 20/
{
	normalize(@func, 5);

	printf("denormalized:");
	denormalize(@func);
	printa(@func);
	exit(0);
}
