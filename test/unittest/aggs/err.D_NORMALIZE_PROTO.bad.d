/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 *	normalize() accepts 2 args - passing fewer is an error.
 *
 * SECTION: Aggregations/Clearing aggregations
 *
 *
 */


#pragma D option quiet
#pragma D option aggrate=1ms
#pragma D option switchrate=1ms

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
/i == 20/
{
	printf("normalized data:\n");
	normalize(@func);
	exit(0);
}
