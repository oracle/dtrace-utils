/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Positive test for dynamic variable size.
 *
 * SECTION: Buffers and Buffering/switch Policy;
 *	Buffers and Buffering/Buffer Sizes;
 *	Options and Tunables/bufsize;
 *	Options and Tunables/bufpolicy;
 *	Options and Tunables/switchrate
 */

#pragma D option dynvarsize=100
#pragma D option quiet

int n;

tick-10ms
/n++ < 100/
{
	a[n] = 1;
}

tick-10ms
/n == 100/
{
	exit(2);
}

END
/a[99]/
{
	exit(1);
}

END
/!a[99]/
{
	exit(0);
}
