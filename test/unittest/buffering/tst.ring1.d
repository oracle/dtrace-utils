/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
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
 *	Options and Tunables/bufpolicy;
 *	Options and Tunables/switchrate
 */

/*
 * We assume that a trace() of an integer stores at least 8 bytes.  If ring
 * buffering is not working properly, this trace() will induce a drop, and the
 * counter won't be incremented.  We set the switchrate to one second just to
 * sure that a high switchrate doesn't mask broken ring buffers.
 *
 * This test used to exit(2) from the 3rd tick-10ms enabling, and have the END
 * probe do the final exit.  However, that poses a problem with consistent test
 * output because if the END probe fires on the same CPU as the tick-10ms probe
 * it will consume some buffer space in the ring buffer, pushing the 295th
 * tick-10ms firing out of the buffer.  If it fires on a sifferent CPU, that
 * 295th firing remains in the buffer.  By performing the final exit right from
 * the 3rd enabling, we can ensure that the content of the buffer is consistent
 * if the tets succeeds.
 */
#pragma D option bufpolicy=ring
#pragma D option bufsize=50
#pragma D option switchrate=1sec

int n;
int i;

tick-10msec
/n < 300/
{
	trace(i);
	i++;
}

tick-10msec
/n < 300/
{
	n++;
}

tick-10msec
/n == 300/
{
	exit(i == 300 ? 0 : 1);
}
