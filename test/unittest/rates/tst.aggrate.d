/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: dtv2 */
/* @@timeout: 15 */

/*
 * ASSERTION:
 *   If the aggregation rate is set properly, there should be no aggregation
 *   drops.
 *
 * SECTION: Aggregations/Minimizing drops;
 *	Options and Tunables/aggrate;
 *	Options and Tunables/aggsize
 */

/*
 * We rely on the fact that at least 30 bytes must be stored per aggregation
 * iteration, but that no more than 300 bytes are stored per iteration.
 * We are going to let this run for ten seconds.  If the aggregation rate
 * is being set properly, there should be no aggregation drops.  Note that
 * this test (regrettably) may be scheduling sensitive -- but it should only
 * fail on the most pathological systems.
 */
#pragma D option aggsize=300
#pragma D option aggrate=10msec
#pragma D option quiet

int n;

tick-100msec
/n < 100/
{
	@a[n++] = sum(n);
}

tick-100msec
/n == 100/
{
	exit(0);
}

END
{
	printa("%10d\n", @a);
}
