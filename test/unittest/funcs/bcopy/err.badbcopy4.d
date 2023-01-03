/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	bcopy should not copy from one memory location to another
 *	if the source memory location is not valid.
 *
 * SECTION: Actions and Subroutines/alloca();
 * 	Actions and Subroutines/bcopy()
 *
 */

#pragma D option quiet

BEGIN
{
	ptr = alloca(sizeof(int));

	/* Attempt to copy from a NULL address */
	bcopy((void *)NULL, ptr, sizeof(int));
	exit(0);
}

ERROR
{
	exit(1);
}
