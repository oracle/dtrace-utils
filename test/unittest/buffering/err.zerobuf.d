/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Test with a bufsize of 0 - should return an error.
 *
 * SECTION:
 *	Buffers and Buffering/Buffer Sizes;
 *	Options and Tunables/bufsize
 */

#pragma D option bufsize=0

BEGIN
{
	exit(0);
}
