/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Calling exit(n) with an expression that evaluates to 0 indicates
 *	      success.
 *
 * SECTION: Actions and Subroutines/exit()
 */

#pragma D option quiet

BEGIN
{
	x = 1;
	exit(x - 1);
}
