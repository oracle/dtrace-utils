/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test multiple aggregations and the default output order
 *
 * SECTION: Aggregations/Aggregations;
 *	Aggregations/Output
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
	@b = sum(i);
	@c = min(i);
	@d = max(i);
	@e = quantize(i);
	@f = lquantize(i, 0, 1000, 100);

	i += 100;
}

tick-10ms
/i == 1000/
{
	exit(0);
}
