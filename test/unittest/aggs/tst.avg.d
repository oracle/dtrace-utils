/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 * 	Positive avg() test
 *
 * SECTION: Aggregations/Aggregations
 *
 * NOTES: This is a simple verifiable positive test of the avg() function.
 */

#pragma D option quiet

BEGIN
{
	i = 0;
}

tick-10ms
/i < 1000/
{
	@a = avg(i);
	i += 100;
}

tick-10ms
/i == 1000/
{
	exit(0);
}
