/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The return() action is implemented as a function call.
 *
 * SECTION: Actions and Subroutines/return()
 */

#pragma D option quiet

BEGIN
{
	return;
	exit(0);
}
