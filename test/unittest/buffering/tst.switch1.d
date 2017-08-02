/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Positive test for switch buffer policy.
 *
 * SECTION: Buffers and Buffering/switch Policy;
 *	Buffers and Buffering/Buffer Sizes;
 *	Options and Tunables/bufsize;
 *	Options and Tunables/bufpolicy;
 *	Options and Tunables/switchrate
 */

/*
 * We assume that a printf() of an integer stores at least 8 bytes, and no more
 * than 16 bytes.
 */
#pragma D option bufpolicy=switch
#pragma D option bufsize=32
#pragma D option switchrate=500msec
#pragma D option quiet

int n;
int i;

tick-1sec
/n < 10/
{
	printf("%d\n", i);
	i++;
}

tick-1sec
/n < 10/
{
	n++;
}

tick-1sec
/n == 10/
{
	exit(0);
}
