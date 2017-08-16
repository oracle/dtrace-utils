/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	breakpoint() should handle arguments passed as an error.
 *
 * SECTION: Actions and Subroutines/breakpoint()
 *
 */


BEGIN
{
	breakpoint(1, 2);
	exit(0);
}

