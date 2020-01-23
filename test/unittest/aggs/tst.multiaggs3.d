/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 *   Test multiple aggregations and overriding default order with
 *   printa() statements.
 *
 * SECTION: Aggregations/Aggregations;
 *	Aggregations/Output
 *
 * NOTES: This is a simple verifiable test.
 *
 */

#pragma D option quiet

BEGIN
{
	i = 0;
}

tick-10ms
/i < 1000/
{
	@a = count();
	@b = avg(i);
	@c = sum(i);
	@d = min(i);
	@e = max(i);
	@f = quantize(i);
	@g = lquantize(i, 0, 1000, 100);

	i += 100;
}

tick-10ms
/i == 1000/
{
	printa("%@d\n", @g);
	printa("%@d\n", @f);
	printa("%@d\n", @e);
	printa("%@d\n", @d);
	printa("%@d\n", @c);
	printa("%@d\n", @b);
	printa("%@d\n", @a);

	exit(0);
}
