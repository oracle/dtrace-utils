/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Multiple aggregates can be used within the same D script.
 *
 * SECTION: Aggregations/Aggregations
 *
 */

#pragma D option quiet

BEGIN
{
	time_1 = timestamp;
	i = 0;
}

tick-10ms
/i <= 10/
{
	time_2 = timestamp;
	new_time = time_2 - time_1;
	@a[pid] = max(new_time);
	@b[pid] = min(new_time);
	@c[pid] = avg(new_time);
	@d[pid] = sum(new_time);
	@e[pid] = quantize(new_time);
	@f[timestamp] = max(new_time);
	@g[timestamp] = quantize(new_time);
	@h[timestamp] = lquantize(new_time, 0, 10000, 1000);
	time_1 = time_2;
	i++;
}

tick-10ms
/i == 10/
{
	exit(0);
}
