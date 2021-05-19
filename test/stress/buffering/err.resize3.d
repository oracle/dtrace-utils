/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@timeout: 500 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 *	Checks that setting "bufresize" to "manual" will cause buffer
 *	allocation to fail for large speculative buffer sizes.
 *
 * SECTION: Buffers and Buffering/Buffer Resizing Policy;
 *	Options and Tunables/bufresize;
 *	Options and Tunables/specsize
 *
 */

#pragma D option quietresize=no
#pragma D option bufresize=manual
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
