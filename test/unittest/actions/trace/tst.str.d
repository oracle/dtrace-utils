/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION: The trace() action accepts a string argument.
 *
 * SECTION: Actions and Subroutines/trace()
 */

#pragma D option quiet

BEGIN
{
	trace("a");
	exit(0);
}
