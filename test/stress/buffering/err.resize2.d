/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@timeout: 500 */

/*
 * ASSERTION:
 *	Checks that setting "bufresize" to "manual" will cause buffer
 *	allocation to fail for large aggregation buffer sizes.
 *
 * SECTION: Buffers and Buffering/Buffer Resizing Policy;
 *	Options and Tunables/bufresize;
 *	Options and Tunables/aggsize
 *
 */

#pragma D option quietresize=no
#pragma D option bufresize=manual
#pragma D option aggsize=100t

BEGIN
{
	@a[probeprov] = count();
	exit(0);
}
