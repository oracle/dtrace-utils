/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Checks that setting "bufresize" to "auto" causes nothing
 *	special for speculative buffer sizes, even if extremely large.
 *
 * SECTION: Buffers and Buffering/Buffer Resizing Policy;
 *	Options and Tunables/specsize;
 *	Options and Tunables/bufresize
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
