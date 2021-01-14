/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: When normalizing multiple times, the last one prevails.
 *
 * SECTION: Aggregations/Data Normalization
 *
 */

#pragma D option quiet

BEGIN
{
	i = 0;
}

tick-10ms
/i < 20/
{
	@a = sum(i * 100);
	@b = sum(i * 100);
	i++;
}

tick-10ms
/i == 20/
{
	normalize(@a, 2);
	normalize(@a, 5);
	exit(0);
}
