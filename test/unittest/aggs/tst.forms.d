/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Positive aggregation test
 *
 * SECTION: Aggregations/Aggregations
 *
 */


#pragma D option quiet

BEGIN
{
	@a = count();
	@b = max(1);
	@c[0] = count();
	@d[0] = max(1);

	printa("\n@a = %@u\n", @a);
	printa("@b = %@u\n", @b);
	printa("@c = %@u\n", @c);
	printa("@d = %@u\n", @d);

	exit(0);
}
