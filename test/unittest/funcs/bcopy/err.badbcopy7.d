/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: bcopy from an alloca pointer is subject to size checks
 *
 * SECTION: Actions and Subroutines/alloca();
 *	    Actions and Subroutines/bcopy()
 */

#pragma D option quiet

BEGIN
{
	d = alloca(20);
	s = alloca(10);
	bcopy(s, d, 20);
	exit(0);
}

ERROR
{
	exit(1);
}
