/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 *   Positive test for fill buffer policy.
 *
 * SECTION: Buffers and Buffering/fill Policy;
 * 	Buffers and Buffering/Buffer Sizes;
 *	Options and Tunables/bufsize;
 *	Options and Tunables/bufpolicy;
 *	Options and Tunables/statusrate
 */
/*
 * This is a brute-force way of testing fill buffers.  We assume that each
 * printf() stores 8 bytes.  Because each fill buffer is per-CPU, we must
 * fill up our buffer in one series of enablings on a single CPU.
 */
#pragma D option bufpolicy=fill
#pragma D option bufsize=64
#pragma D option statusrate=10ms
#pragma D option quiet

int i;

tick-10ms
{
	printf("%d\n", i++);
}

tick-10ms
{
	printf("%d\n", i++);
}

tick-10ms
{
	printf("%d\n", i++);
}

tick-10ms
{
	printf("%d\n", i++);
}

tick-10ms
{
	printf("%d\n", i++);
}

tick-10ms
{
	printf("%d\n", i++);
}

tick-10ms
{
	printf("%d\n", i++);
}

tick-10ms
{
	printf("%d\n", i++);
}

tick-10ms
{
	printf("%d\n", i++);
}

tick-10ms
{
	printf("%d\n", i++);
}

tick-10ms
{
	printf("%d\n", i++);
}

tick-10ms
{
	printf("%d\n", i++);
}

tick-10ms
/i >= 100/
{
	exit(1);
}
