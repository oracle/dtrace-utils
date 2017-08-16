/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	panic() should handle any argument passed as an error.
 *
 * SECTION: Actions and Subroutines/panic()
 *
 */


BEGIN
{
	panic("badarg");
	exit(0);
}

