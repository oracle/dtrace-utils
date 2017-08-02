/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	alloca() should handle no arguments as an error
 *
 * SECTION: Actions and Subroutines/alloca()
 *
 */

#pragma D option quiet



BEGIN
{
	ptr = alloca();
	exit(1);
}
