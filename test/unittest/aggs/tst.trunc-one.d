/*
 * Oracle Linux DTrace.
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Truncating one aggregation truncates only that one.
 *
 * SECTION: Aggregations/Clearing aggregations
 */
/* @@nosort */

#pragma D option quiet

BEGIN
{
	@a[1] = sum(100);
	@a[2] = sum( 20);
	@a[3] = sum(  3);
	@b[4] = sum(400);
	@b[5] = sum( 50);
	@b[6] = sum(  6);
	@c[7] = sum(700);
	@c[8] = sum( 80);
	@c[9] = sum(  9);
	trunc(@b);
	printa(@a);
	printa(@b);
	printa(@c);
	exit(0);
}
