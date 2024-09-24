/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Checks that setting "bufresize" to "manual" will not cause buffer
 *	allocation to fail even for large speculative buffer sizes.
 *	(With the current implementation, speculative data is written to
 *	the principal buffer.  The consumer reads the data and stores it
 *	in growable, linked lists.  The specsize limit is checked for
 *	read data, but a large specsize is not rejected.)
 *
 * SECTION: Buffers and Buffering/Buffer Resizing Policy;
 *	Options and Tunables/bufresize;
 *	Options and Tunables/specsize
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
	trace(12345678);
}

BEGIN
{
	commit(spec);
}

BEGIN
{
	exit(0);
}
