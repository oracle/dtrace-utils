/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Empty aggregations are not printed
 *
 * SECTION: Aggregations/Aggregations
 */

#pragma D option quiet

BEGIN
{
	i = 12345678;
	@a = sum(i);
	exit(0);
}

/* this probe should not fire */
tick-1hour
{
	/* these aggregations should not be printed */
	@b = min(i);
	@c = max(i);
}
