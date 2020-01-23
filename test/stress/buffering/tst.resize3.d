/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */
/* @@timeout: 500 */

/*
 * ASSERTION:
 *	Checks that setting "bufresize" to "auto" will cause buffer
 *	allocation to succeed, even for large speculative buffer sizes.
 *
 * SECTION: Buffers and Buffering/Buffer Resizing Policy;
 *	Options and Tunables/specsize;
 *	Options and Tunables/bufresize
 *
 * NOTES:
 *	On some small memory machines, this test may consume so much memory
 *	that it induces memory allocation failure in the dtrace library.  This
 *	will manifest itself as an error like one of the following:
 *
 *	    dtrace: processing aborted: Memory allocation failure
 *	    dtrace: could not enable tracing: Memory allocation failure
 *
 *	These actually indicate that the test performed as expected; failures
 *	of the above nature should therefore be ignored.
 *
 */

#pragma D option quietresize=no
#pragma D option bufresize=auto
#pragma D option specsize=100t

BEGIN
{
	spec = speculation();
}

BEGIN
{
	speculate(spec);
	trace(epid);
}

BEGIN
{
	commit(spec);
}

BEGIN
{
	exit(0);
}
