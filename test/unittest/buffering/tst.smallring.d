/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Positive test for ring buffer policy.
 *
 * SECTION: Buffers and Buffering/ring Policy;
 *	Buffers and Buffering/Buffer Sizes;
 *	Options and Tunables/bufsize;
 *	Options and Tunables/bufpolicy
 */

#pragma D option bufpolicy=ring
#pragma D option bufsize=16

tick-10ms
{
	trace(0xbadbaddefec8d);
}

tick-10ms
/0/
{
	trace((int)1);
}

tick-10ms
/i++ > 20/
{
	exit(0);
}
