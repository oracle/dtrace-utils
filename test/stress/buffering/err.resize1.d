/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@timeout: 500 */

/*
 * ASSERTION:
 *	Checks that setting "bufresize" to "manual" will cause buffer
 *	allocation to fail for large principal buffer sizes.
 *
 * SECTION: Buffers and Buffering/Buffer Resizing Policy;
 *	Options and Tunables/bufsize;
 *	Options and Tunables/bufresize
 *
 */

#pragma D option quietresize=no
#pragma D option bufresize=manual
#pragma D option bufsize=100t

BEGIN
{
	exit(0);
}
