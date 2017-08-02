/*
 * Oracle Linux DTrace.
 * Copyright © 2008, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *      Positive stddev() test
 *
 * SECTION: Aggregations/Aggregations
 *
 * NOTES: This is a simple verifiable positive test of the stddev() function.
 */

#pragma D option quiet

BEGIN
{
	@a = stddev(5000000000);
	@a = stddev(5000000100);
	@a = stddev(5000000200);
	@a = stddev(5000000300);
	@a = stddev(5000000400);
	@a = stddev(5000000500);
	@a = stddev(5000000600);
	@a = stddev(5000000700);
	@a = stddev(5000000800);
	@a = stddev(5000000900);
	@b = stddev(-5000000000);
	@b = stddev(-5000000100);
	@b = stddev(-5000000200);
	@b = stddev(-5000000300);
	@b = stddev(-5000000400);
	@b = stddev(-5000000500);
	@b = stddev(-5000000600);
	@b = stddev(-5000000700);
	@b = stddev(-5000000800);
	@b = stddev(-5000000900);
	exit(0);
}
