/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	bcopy should not copy from one memory location to another
 *	if the memory is not properly allocated
 *
 * SECTION: Actions and Subroutines/alloca();
 * 	Actions and Subroutines/bcopy()
 *
 */

#pragma D option quiet


BEGIN
{
	ptr = alloca(0);

	/* Attempt to bcopy to scratch memory that isn't allocated */
	bcopy((void *)&`max_pfn, ptr, sizeof(unsigned long));
	exit(0);
}

ERROR
{
	exit(1);
}
