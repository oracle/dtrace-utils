/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Simple profile provider test.
 *	call profile<provider name>; called with no ":".
 *	should fail with compilation error.
 *
 * SECTION: profile Provider/tick-n probes
 *
 */

#pragma D option quiet

BEGIN
{
	i = 0;
}

profiletick-1sec
{
	trace(i);
}
