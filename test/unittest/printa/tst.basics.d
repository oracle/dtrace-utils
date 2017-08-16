/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test the basic formatting of all the supported kinds of aggregations.
 *
 * SECTION: Output Formatting/printa()
 *
 */

#pragma D option quiet

BEGIN
{
	@a = avg(1);
	@b = count();
	@c = lquantize(1, 1, 10);
	@d = max(1);
	@e = min(1);
	@f = sum(1);
	@g = quantize(1);

	printa("@a = %@u\n", @a);
	printa("@b = %@u\n", @b);
	printa("@c = %@d\n", @c);
	printa("@d = %@u\n", @d);
	printa("@e = %@u\n", @e);
	printa("@f = %@u\n", @f);
	printa("@g = %@d\n", @g);

	exit(0);
}
