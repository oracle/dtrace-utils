/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 - exit causes duplication of output */

/*
 * ASSERTION:
 *   Positive test for ring buffer policy.
 *
 * SECTION: Buffers and Buffering/ring Policy;
 * SECTION: Buffers and Buffering/Buffer Sizes;
 *	Options and Tunables/bufsize;
 *	Options and Tunables/bufpolicy
 */

#pragma D option bufpolicy=ring
#pragma D option bufsize=512k
#pragma D option quiet

tick-1sec
/n < 5/
{
	printf("%d\n", n++);
}

tick-1sec
/n == 5/
{
	exit(0);
}
