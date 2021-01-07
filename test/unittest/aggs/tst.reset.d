/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Printing aggregations repeatedly should reproduce results
 *
 * SECTION: Aggregations/Aggregations
 */

#pragma D option quiet

BEGIN
{
	i = 10;
	j = 20;
	k = 30;
	l = 40;
	m = 50;
	@a = sum(i); @b = stddev(i); @c = count(); @d = avg(i); @e = quantize(i);
	@a = sum(j); @b = stddev(j); @c = count(); @d = avg(j); @e = quantize(j);
	@a = sum(k); @b = stddev(k); @c = count(); @d = avg(k); @e = quantize(k);
	@a = sum(l); @b = stddev(l); @c = count(); @d = avg(l); @e = quantize(l);
	@a = sum(m); @b = stddev(m); @c = count(); @d = avg(m); @e = quantize(m);
	exit(0)
}

END
{
	printa(@a); printa(@b); printa(@c); printa(@d); printa(@e);
	printa(@a); printa(@b); printa(@c); printa(@d); printa(@e);
	printa(@a); printa(@b); printa(@c); printa(@d); printa(@e);
	printa(@a); printa(@b); printa(@c); printa(@d); printa(@e);
	printa(@a); printa(@b); printa(@c); printa(@d); printa(@e);
}
