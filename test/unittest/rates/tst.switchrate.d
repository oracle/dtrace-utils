/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@timeout: 15 */

/*
 * ASSERTION:
 *   If the switch rate is set properly, there should be no drops.
 *
 * SECTION: Buffers and Buffering/switch Policy;
 *	Options and Tunables/bufsize;
 *	Options and Tunables/switchrate
 */

/*
 * We rely on the fact that at least 8 bytes must be stored per iteration
 * (EPID plus data), but that no more than 40 bytes are stored per iteration.
 * We are going to let this run for ten seconds.  If the switch rate
 * is being set properly, there should be no drops.  Note that this test
 * (regrettably) may be scheduling sensitive -- but it should only fail on
 * the most pathological systems.
 */
#pragma D option bufsize=40
#pragma D option switchrate=10msec
#pragma D option quiet

int n;

tick-100msec
/n < 100/
{
	printf("%10d\n", n++);
}

tick-100msec
/n == 100/
{
	exit(0);
}
