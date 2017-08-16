/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@timeout: 500 */

/*
 * ASSERTION:
 *	Checks that setting "bufresize" to "auto" will cause buffer
 *	allocation to succeed, even for large aggregation buffer sizes.
 *
 * SECTION: Buffers and Buffering/Buffer Resizing Policy;
 *	Options and Tunables/aggsize;
 *	Options and Tunables/bufresize
 *
 * NOTES:
 *	We use the undocumented "preallocate" option to make sure dtrace(1M)
 *	has enough space in its heap to allocate a buffer as large as the
 *	kernel's trace buffer.
 */

#pragma D option quietresize=no
#pragma D option preallocate=100t
#pragma D option bufresize=auto
#pragma D option aggsize=100t

BEGIN
{
	@a[probeprov] = count();
	exit(0);
}
