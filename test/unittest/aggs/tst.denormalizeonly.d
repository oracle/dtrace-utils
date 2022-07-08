/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   denormalize() should work even if normalize() isn't called.
 *
 * SECTION: Aggregations/Normalization
 *
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
	@func[i%5] = sum(i * 100);
	i++;
}

tick-10ms
/i == 20/
{
	printf("denormalized:");
	denormalize(@func);
	exit(0);
}
